// Measures the two parts of the model that could get slow: the path search
// that the eyes of a ghost use, and the whole simulation at 120 steps per
// second. Both are measured against the real level and against open mazes
// that are much larger than anything the game ships, so that the scaling is
// visible and "a bigger level would be slow" can be answered with a number
// instead of a guess.
//
// Usage: pacman_bench [--iterations N]     (N = 20000 searches per maze by default)

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "pacman/Engine.hpp"
#include "pacman/Game.hpp"
#include "pacman/Level.hpp"
#include "pacman/Maze.hpp"

using namespace pac;

namespace {

using Clock = std::chrono::steady_clock;

double secondsSince(const Clock::time_point start) {
    return std::chrono::duration<double>(Clock::now() - start).count();
}

// A small deterministic generator, so that two runs of the benchmark
// measure the same work.
class Rng {
public:
    int below(int n) {
        state_ = state_ * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<int>((state_ >> 33) % static_cast<std::uint64_t>(n));
    }

private:
    std::uint64_t state_ = 0x243F6A8885A308D3ULL;
};

// A map of size cols x rows with nothing but open tiles: the worst case
// for the path search, which then has to walk over every cell. The four
// ghosts are put on the four corners, because a map without ghost markers
// makes the parser put all of them on the first free tile — and then they
// would start the eyes scenario standing on their own target.
std::vector<std::string> openMap(const int cols, const int rows) {
    std::vector<std::string> map(static_cast<std::size_t>(rows),
                                 std::string(static_cast<std::size_t>(cols), '.'));
    map[1].replace(1, 1, "P");  // the parser wants exactly one player
    map[rows - 2].replace(1, 1, "1");
    map[1].replace(cols - 2, 1, "2");
    map[rows - 2].replace(cols - 2, 1, "3");
    map[rows / 2].replace(cols / 2, 1, "4");
    return map;
}

bool parseOpenLevel(const int cols, const int rows, Level& out) {
    std::string error;
    if (!Level::parse(openMap(cols, rows), out, &error)) {
        std::cerr << "cannot parse an open " << cols << "x" << rows << " map: " << error << "\n";
        return false;
    }
    return true;
}

struct Coord {
    int x = 0;
    int y = 0;
};

struct SearchStats {
    double neighbourNs = 0.0;  // to the next tile: what a ghost pays at almost every decision
    double averageNs = 0.0;    // between two random walkable tiles
    double cornerNs = 0.0;     // from one corner of the walkable area to the other
    long long sink = 0;        // the sum of the directions found: proof that the work was done
};

SearchStats timePathSearch(Maze& maze, const int iterations) {
    // Every walkable tile, in row-major order. The first and the last are
    // the pair that makes the search visit the most cells.
    std::vector<Coord> walkable;
    for (int y = 0; y < maze.rows(); ++y) {
        for (int x = 0; x < maze.cols(); ++x) {
            if (maze.canEnter(x, y, true)) {
                walkable.push_back(Coord{x, y});
            }
        }
    }

    SearchStats stats;
    if (walkable.size() < 2) {
        return stats;
    }

    // The shortest search there is: the goal is the tile next door. What it
    // costs is what the search costs when it has nothing to walk over, so
    // the difference to the other rows is the walk itself.
    Coord neighbourFrom;
    Coord neighbourTo;
    bool haveNeighbour = false;
    for (const Coord tile : walkable) {
        if (tile.x + 1 < maze.cols() && maze.canEnter(tile.x + 1, tile.y, true)) {
            neighbourFrom = tile;
            neighbourTo = Coord{tile.x + 1, tile.y};
            haveNeighbour = true;
            break;
        }
    }

    const auto randomTile = [&walkable, &maze](Rng& rng) -> Coord {
        return walkable[static_cast<std::size_t>(rng.below(static_cast<int>(walkable.size())))];
    };

    Rng rng;
    for (int i = 0; i < 32; ++i) {  // warm-up: the first call pays for the cache lines
        const Coord from = randomTile(rng);
        const Coord to = randomTile(rng);
        stats.sink += static_cast<int>(maze.stepToward(from.x, from.y, to.x, to.y, true));
    }

    if (haveNeighbour) {
        const Clock::time_point neighbourStart = Clock::now();
        for (int i = 0; i < iterations; ++i) {
            stats.sink += static_cast<int>(
                maze.stepToward(neighbourFrom.x, neighbourFrom.y, neighbourTo.x, neighbourTo.y, true));
        }
        stats.neighbourNs = secondsSince(neighbourStart) * 1.0e9 / iterations;
    }

    const Clock::time_point averageStart = Clock::now();
    for (int i = 0; i < iterations; ++i) {
        const Coord from = randomTile(rng);
        const Coord to = randomTile(rng);
        stats.sink += static_cast<int>(maze.stepToward(from.x, from.y, to.x, to.y, true));
    }
    stats.averageNs = secondsSince(averageStart) * 1.0e9 / iterations;

    const Coord first = walkable.front();
    const Coord last = walkable.back();
    const Clock::time_point cornerStart = Clock::now();
    for (int i = 0; i < iterations; ++i) {
        stats.sink += static_cast<int>(maze.stepToward(first.x, first.y, last.x, last.y, true));
    }
    stats.cornerNs = secondsSince(cornerStart) * 1.0e9 / iterations;

    return stats;
}

struct SimStats {
    double stepsPerSecond = 0.0;  // over the steps where the game was actually simulating
    int restarts = 0;             // deaths that ended the run before it was over
    long long sink = 0;
};

// Steps the game the way a view would: 120 steps per second, a new input
// request every now and then, and a restart when the game is over, so that
// the loop never measures an idle game.
//
// Only the steps taken while the game is running are counted, and so is the
// wall-clock time they took: the death and level-complete pauses are real
// time during which the model does almost nothing, and counting them would
// make a scenario that dies often look like a fast one.
SimStats timeSimulation(const Level& level, const int steps, const bool eyesHeavy) {
    Game game(level);
    Engine engine(game);
    game.start();

    Rng rng;
    SimStats stats;
    double runningSeconds = 0.0;
    long long runningSteps = 0;
    bool wasRunning = game.isRunning();
    auto lastTick = Clock::now();
    for (int i = 0; i < steps; ++i) {
        if (game.isOver()) {
            game.reset();
            ++stats.restarts;
        }
        if (game.state() == GameState::Ready) {
            game.start();
        }
        if (i % 37 == 0) {
            game.player().requestDirection(kDirections[rng.below(4)]);
        }
        if (eyesHeavy && i % 300 == 0) {
            // The eyes of a ghost are the only thing in the model that runs
            // the path search, so four ghosts coming home at once is the
            // most a game can ask for.
            for (Ghost& ghost : game.ghosts()) {
                ghost.setMode(GhostMode::Eyes);
            }
        }
        engine.update(Engine::kStepSeconds);

        const auto now = Clock::now();
        if (wasRunning) {
            runningSeconds += std::chrono::duration<double>(now - lastTick).count();
            ++runningSteps;
        }
        lastTick = now;
        wasRunning = game.isRunning();
        stats.sink += game.score() + game.pelletsRemaining();
    }

    if (runningSeconds > 0.0) {
        stats.stepsPerSecond = static_cast<double>(runningSteps) / runningSeconds;
    }
    return stats;
}

}  // namespace

int main(int argc, char** argv) {
    int iterations = 20000;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--iterations" && i + 1 < argc) {
            iterations = std::atoi(argv[++i]);
            if (iterations < 1) {
                iterations = 1;
            }
        }
    }

    Level level;
    std::string error;
    const std::string levelPath = std::string(PACMAN_LEVELS_DIR) + "/level1.txt";
    if (!Level::loadFile(levelPath, level, &error)) {
        std::cerr << "cannot load " << levelPath << ": " << error << "\n";
        return 1;
    }

    struct Case {
        std::string name;
        Level level;
    };

    std::vector<Case> cases;
    cases.push_back(Case{"level1", level});
    for (const std::pair<int, int>& size : {std::make_pair(10, 10), std::make_pair(21, 21),
                                            std::make_pair(50, 50), std::make_pair(100, 100)}) {
        Level open;
        if (!parseOpenLevel(size.first, size.second, open)) {
            return 1;
        }
        cases.push_back(Case{"open " + std::to_string(size.first) + "x" +
                                 std::to_string(size.second),
                             std::move(open)});
    }

    std::cout << "path search, nanoseconds per call (" << iterations << " calls per row)\n\n";
    std::cout << "  maze                  cells    next tile    two random tiles    corner to corner\n";

    long long sink = 0;
    for (Case& testCase : cases) {
        Maze maze(std::move(testCase.level));
        const SearchStats stats = timePathSearch(maze, iterations);
        sink += stats.sink;
        std::cout << "  " << std::left << std::setw(20) << testCase.name << std::right << std::setw(6)
                  << (maze.cols() * maze.rows()) << std::setw(12) << std::fixed
                  << std::setprecision(1) << stats.neighbourNs << " ns" << std::setw(12)
                  << stats.averageNs << " ns" << std::setw(12) << stats.cornerNs << " ns\n";
    }

    std::cout << "\nsimulation, running steps per second (the budget is 120)\n\n";
    std::cout << "  scenario                         steps/s    times the budget    restarts\n";

    Level open50;
    if (!parseOpenLevel(50, 50, open50)) {
        return 1;
    }

    struct Scenario {
        std::string name;
        const Level* level;
        bool eyesHeavy;
    };
    const std::vector<Scenario> scenarios = {
        {"level1, normal play", &level, false},
        {"level1, four ghosts as eyes", &level, true},
        {"open 50x50, four ghosts as eyes", &open50, true},
    };
    for (const Scenario& scenario : scenarios) {
        const SimStats stats = timeSimulation(*scenario.level, iterations * 6, scenario.eyesHeavy);
        sink += stats.sink;
        std::cout << "  " << std::left << std::setw(32) << scenario.name << std::right << std::fixed
                  << std::setprecision(0) << std::setw(9) << stats.stepsPerSecond << std::setw(19)
                  << (stats.stepsPerSecond / 120.0) << std::setw(11) << stats.restarts << "\n";
    }

    // The searches and the steps cannot be thrown away, because their
    // results end up here.
    std::cout << "\nchecked: " << sink << "\n";
    return 0;
}
