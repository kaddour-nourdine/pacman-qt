#include "Fixtures.hpp"
#include "TestFramework.hpp"

#include "pacman/Game.hpp"
#include "pacman/Maze.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

// Determinism regression.
//
// The model is the heart of the project: the same inputs must always produce
// the same game, on every machine and in every build. The view relies on it
// (the interpolation, the replay tooling, the tests that compare states), so
// a change that silently alters the simulation has to be caught.
//
// The test replays a recorded input log against a fixed level and a fixed
// random seed and digests the whole trajectory. Three things are checked:
//
//   1. two identical replays produce the same digest (nothing in the model
//      depends on the clock, on memory addresses or on an unseeded random
//      source),
//   2. the digest matches a golden value (the behaviour did not drift),
//   3. the replay really exercises the game (otherwise the golden value
//      would pin a game that does nothing).
//
// The digest has two parts:
//
//   - the trajectory: the discrete state (tiles, directions, modes, counters)
//     after every step. All of it is exact, so it does not depend on how a
//     compiler rounds a float.
//   - the final state: the positions as well. They are floats, so they are
//     quantized before they are hashed; a different rounding on another
//     platform can still change the last bit.
//
// The golden value is therefore tied to the toolchain. Regenerate it when a
// rule changes on purpose -- and only then.

namespace {

// The fixed step of the engine (Engine::kStepSeconds): the replay uses the
// same one, so the model sees exactly the sequence of steps it sees in the
// game.
constexpr float kStepSeconds = 1.0f / 120.0f;

// The seed of the replay. Every random decision of the ghosts (the
// tie-break between two equally good directions) is drawn from it.
constexpr std::uint32_t kSeed = 0x5eed1234u;

// How long the replay runs: 25 seconds of game time.
constexpr int kSteps = 3000;

// The golden digest of the replay. See the note at the top of the file.
constexpr std::uint64_t kGoldenDigest = 0xe6f19552a59176cbull;

// The level the replay is played on.
//
// It is written down here instead of being loaded from levels/ on purpose:
// the golden value pins the *model*, not the content, so adding or editing a
// level does not break this test.
//
// It has everything the replay needs: a ghost house with a door, ghosts that
// have to tie-break between two directions (that is where the random source
// is used), a power pellet (frightened mode) and a fruit.
std::vector<std::string> replayMap() {
    return {
        "name = Replay",
        "playerSpeed = 6.0",
        "ghostSpeed = 5.0",
        "frightenedSpeed = 3.5",
        "frightenedSeconds = 5.0",
        "modeSchedule = 7,20",
        "",
        "#############",
        "#....P......#",
        "#.###.#.###.#",
        "#.#-----#.#.#",
        "#.#123  #.#.#",
        "#.#######.#.#",
        "#....4..o..F#",
        "#############",
    };
}

// FNV-1a: a small, dependency-free digest. It only has to be stable, not
// cryptographic.
class Digest {
public:
    void add(std::uint64_t value) {
        for (int byte = 0; byte < 8; ++byte) {
            state_ ^= static_cast<std::uint8_t>((value >> (byte * 8)) & 0xffu);
            state_ *= 1099511628211ull;
        }
    }

    std::uint64_t value() const { return state_; }

private:
    std::uint64_t state_ = 1469598103934665603ull;
};

// The positions are floats, so they are quantized before they are hashed:
// the last-bit differences between compilers must not change the digest,
// while a real behaviour change (a different path, a different tie-break)
// still must. One tile is 64 units, and an entity moves at least 3.5/120 of
// a tile per step, so a single step of drift is still visible.
std::int64_t quantized(float value, float scale) {
    return static_cast<std::int64_t>(std::llround(static_cast<double>(value) * static_cast<double>(scale)));
}

// The discrete part of the state: everything that is an exact number.
void addDiscreteState(Digest& digest, const pac::Game& game) {
    digest.add(static_cast<std::uint64_t>(game.state()));
    digest.add(static_cast<std::uint64_t>(game.score()));
    digest.add(static_cast<std::uint64_t>(game.lives()));
    digest.add(static_cast<std::uint64_t>(game.levelNumber()));
    digest.add(static_cast<std::uint64_t>(game.globalMode()));
    digest.add(static_cast<std::uint64_t>(game.pelletsRemaining()));

    const pac::Player& player = game.player();
    digest.add(static_cast<std::uint64_t>(player.tileX()));
    digest.add(static_cast<std::uint64_t>(player.tileY()));
    digest.add(static_cast<std::uint64_t>(player.direction()));
    digest.add(static_cast<std::uint64_t>(player.requestedDirection()));
    digest.add(static_cast<std::uint64_t>(player.pelletsEaten()));
    digest.add(static_cast<std::uint64_t>(player.powerPelletsEaten()));

    for (const pac::Ghost& ghost : game.ghosts()) {
        digest.add(static_cast<std::uint64_t>(ghost.tileX()));
        digest.add(static_cast<std::uint64_t>(ghost.tileY()));
        digest.add(static_cast<std::uint64_t>(ghost.direction()));
        digest.add(static_cast<std::uint64_t>(ghost.mode()));
        digest.add(static_cast<std::uint64_t>(ghost.isReleased() ? 1u : 0u));
        digest.add(static_cast<std::uint64_t>(ghost.pointsValue()));
    }
}

// The positions: floats, and therefore quantized.
void addPositions(Digest& digest, const pac::Game& game) {
    const pac::Player& player = game.player();
    digest.add(static_cast<std::uint64_t>(quantized(player.position().x, 64.0f)));
    digest.add(static_cast<std::uint64_t>(quantized(player.position().y, 64.0f)));

    for (const pac::Ghost& ghost : game.ghosts()) {
        digest.add(static_cast<std::uint64_t>(quantized(ghost.position().x, 64.0f)));
        digest.add(static_cast<std::uint64_t>(quantized(ghost.position().y, 64.0f)));
        digest.add(static_cast<std::uint64_t>(quantized(ghost.target().x, 64.0f)));
        digest.add(static_cast<std::uint64_t>(quantized(ghost.target().y, 64.0f)));
    }

    digest.add(static_cast<std::uint64_t>(quantized(game.frightenedSecondsLeft(), 1000.0f)));
    for (const pac::Ghost& ghost : game.ghosts()) {
        digest.add(static_cast<std::uint64_t>(quantized(ghost.frightenedTimeLeft(), 1000.0f)));
    }
}

// The pellets that are still on the floor.
void addPellets(Digest& digest, const pac::Game& game) {
    const pac::Maze& maze = game.maze();
    for (int y = 0; y < maze.rows(); ++y) {
        for (int x = 0; x < maze.cols(); ++x) {
            digest.add(static_cast<std::uint64_t>(maze.tileAt(x, y)));
        }
    }
}

// One entry of the recorded input log: "at this step, ask for this
// direction". Between two entries the last request stays in place, so the
// player keeps walking until it meets the next request.
struct Input {
    int step;
    pac::Direction direction;
};

const std::vector<Input>& inputLog() {
    static const std::vector<Input> log = {
        {0, pac::Direction::Right},    // along the top corridor to x = 9
        {70, pac::Direction::Down},    // down the right corridor
        {170, pac::Direction::Left},   // onto the power pellet
        {260, pac::Direction::Up},     // back up, to the fruit corridor
        {420, pac::Direction::Left},   // to the fruit
        {560, pac::Direction::Up},
        {700, pac::Direction::Right},
        {860, pac::Direction::Down},
        {1020, pac::Direction::Left},
        {1200, pac::Direction::Up},
        {1400, pac::Direction::Right},
        {1600, pac::Direction::Down},
        {1800, pac::Direction::Left},
        {2000, pac::Direction::Up},
        {2200, pac::Direction::Right},
        {2400, pac::Direction::Down},
        {2600, pac::Direction::Left},
    };
    return log;
}

// What the replay produced, besides the digest.
struct ReplayResult {
    std::uint64_t digest = 0;
    int score = 0;
    int pelletsEaten = 0;
    int powerPelletsEaten = 0;
    int frightenedSteps = 0;
    int deaths = 0;
    int gameOvers = 0;
    int distinctGhostModes = 0;
    pac::GameState finalState = pac::GameState::Ready;
};

ReplayResult replay(const pac::Level& level, std::uint32_t seed, int steps) {
    pac::Game game(level);
    game.setRandomSeed(seed);
    game.start();

    ReplayResult result;
    const pac::Maze& maze = game.maze();
    const int pelletsAtStart = maze.pelletsTotal();

    Digest trajectory;
    const std::vector<Input>& log = inputLog();
    std::size_t next = 0;
    pac::Direction held = pac::Direction::None;
    bool wasAlive = true;
    int ghostModes = 0;

    for (int step = 0; step < steps; ++step) {
        while (next < log.size() && log[next].step == step) {
            held = log[next].direction;
            game.player().requestDirection(held);
            ++next;
        }

        // A death puts the game back into the "ready" state; the player
        // presses a key and the game continues. The replay does the same,
        // and keeps the direction it was holding.
        if (game.state() == pac::GameState::Ready && step > 0) {
            game.start();
            game.player().requestDirection(held);
        }

        game.step(kStepSeconds);

        addDiscreteState(trajectory, game);

        if (game.isFrightened()) {
            ++result.frightenedSteps;
        }
        for (const pac::Ghost& ghost : game.ghosts()) {
            ghostModes |= 1 << static_cast<int>(ghost.mode());
        }
        const bool alive = game.state() != pac::GameState::PlayerDied;
        if (wasAlive && !alive) {
            ++result.deaths;
        }
        wasAlive = alive;
        if (game.state() == pac::GameState::GameOver) {
            ++result.gameOvers;
        }
        // The counters of the player are reset with every life, so the
        // highest number seen is the interesting one.
        result.powerPelletsEaten = std::max(result.powerPelletsEaten, game.player().powerPelletsEaten());
    }

    Digest digest = trajectory;
    addPositions(digest, game);
    addPellets(digest, game);

    result.digest = digest.value();
    result.score = game.score();
    result.pelletsEaten = pelletsAtStart - game.pelletsRemaining();
    result.distinctGhostModes = ghostModes;
    result.finalState = game.state();
    return result;
}

}  // namespace

// Two identical replays must produce the same digest. This is the check
// against a hidden dependency on the clock, on memory addresses or on an
// unseeded random source.
PAC_TEST(Determinism, replayIsReproducible) {
    const pac::Level level = pactest::makeLevel(replayMap(), __FILE__, __LINE__);
    const ReplayResult first = replay(level, kSeed, kSteps);
    const ReplayResult second = replay(level, kSeed, kSteps);
    CHECK_EQ(first.digest, second.digest);
    CHECK_EQ(first.score, second.score);
    CHECK_EQ(first.deaths, second.deaths);
}

// The golden value: the behaviour of the model must not drift.
PAC_TEST(Determinism, replayMatchesTheGoldenDigest) {
    const pac::Level level = pactest::makeLevel(replayMap(), __FILE__, __LINE__);
    const ReplayResult result = replay(level, kSeed, kSteps);
    CHECK_EQ(result.digest, kGoldenDigest);
}

// The seed has to matter: the ghosts break ties between two equally good
// directions at random, so a different seed has to lead to a different game.
PAC_TEST(Determinism, theSeedChangesTheGame) {
    const pac::Level level = pactest::makeLevel(replayMap(), __FILE__, __LINE__);
    const ReplayResult first = replay(level, kSeed, kSteps);
    const ReplayResult other = replay(level, kSeed + 1u, kSteps);
    CHECK(first.digest != other.digest);
}

// The fixture has to stay meaningful: if the level or the input log stopped
// working, the golden digest would still match while testing nothing.
PAC_TEST(Determinism, theReplayExercisesTheGame) {
    const pac::Level level = pactest::makeLevel(replayMap(), __FILE__, __LINE__);
    const ReplayResult result = replay(level, kSeed, kSteps);

    CHECK(result.pelletsEaten > 0);
    CHECK(result.score > 0);
    CHECK(result.powerPelletsEaten >= 1);
    CHECK(result.frightenedSteps > 0);
    CHECK(result.deaths > 0);
    // Scatter, chase and frightened.
    CHECK((result.distinctGhostModes & (1 << static_cast<int>(pac::GhostMode::Scatter))) != 0);
    CHECK((result.distinctGhostModes & (1 << static_cast<int>(pac::GhostMode::Chase))) != 0);
    CHECK((result.distinctGhostModes & (1 << static_cast<int>(pac::GhostMode::Frightened))) != 0);
}
