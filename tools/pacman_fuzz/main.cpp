// A fuzzer for the level parser.
//
// Level::parse is a pure function: it takes lines and either fills a Level
// or explains why it cannot. That makes it the easiest part of the project
// to fuzz -- and the part where a crash would hurt most, because the parser
// is the first thing that touches a file that came from outside.
//
// The tool generates random headers and maps, mutates the levels that ship
// with the game, and checks the promise the parser makes for every input:
//
//     parse either succeeds and produces a level that agrees with the input,
//     or it fails with a message that says why. It never crashes, it never
//     throws, and a failed parse leaves the output level alone.
//
// Usage:
//     pacman_fuzz                             20000 random cases, fixed seed
//     pacman_fuzz --iterations 200000 --seed 7
//     pacman_fuzz --corpus levels             check every file in a directory
//     pacman_fuzz --stdin                     read cases from stdin; a line
//                                             with only "%%" starts a new one

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "pacman/Level.hpp"
#include "pacman/Random.hpp"

namespace {

namespace fs = std::filesystem;

constexpr int kDefaultIterations = 20000;
constexpr std::uint32_t kDefaultSeed = 20240521u;

// The characters a map row may hold, plus a few near misses: a ghost marker
// the parser does not know, a letter, a digit, a tab. The invalid ones are
// there to reach the rejection paths as often as the valid ones.
const char* const kMapAlphabet = "#. -o1234PFf_5x\t@";
constexpr int kMapAlphabetSize = 17;

// The header keys the parser knows, plus a few it does not, and the same
// key in the wrong case.
const char* const kHeaderKeys[] = {
    "name",         "playerSpeed", "ghostSpeed",    "frightenedSpeed", "tunnelSpeed",
    "eyesSpeed",    "modeSchedule", "startingLives", "frightenedSeconds", "Name",
    "playerspeed",  "unknownKey", "",
};

// Values that are valid, values that are not, and values that are only
// valid for some keys.
const char* const kHeaderValues[] = {
    "Fuzz", "1", "0", "-1", "6.0", "1e3", "inf", "nan", "abc", "", " ", "1,2,3", "1,", ",1", "0.0001",
};

// A valid level, used to fill the output of a parse so that a failing parse
// can be caught changing it.
const std::vector<std::string>& seedLevel() {
    static const std::vector<std::string> lines = {
        "name = Seed",
        "playerSpeed = 6.0",
        "",
        "#########",
        "#...P...#",
        "#.1234..#",
        "#.......#",
        "#########",
    };
    return lines;
}

std::string trim(const std::string& text) {
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return std::string();
    }
    const std::size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

// ---------------------------------------------------------------------------
// The promise
// ---------------------------------------------------------------------------

// Everything the caller can see of a level. Two levels with the same
// snapshot are the same level as far as the outside is concerned.
struct Snapshot {
    std::string name;
    int cols = 0;
    int rows = 0;
    int pellets = 0;
    int playerX = 0;
    int playerY = 0;
    pac::Direction direction = pac::Direction::Left;

    static Snapshot of(const pac::Level& level) {
        Snapshot snapshot;
        snapshot.name = level.name();
        snapshot.cols = level.cols();
        snapshot.rows = level.rows();
        snapshot.pellets = level.pelletCount();
        snapshot.playerX = level.playerX();
        snapshot.playerY = level.playerY();
        snapshot.direction = level.playerDirection();
        return snapshot;
    }

    bool operator==(const Snapshot& other) const {
        return name == other.name && cols == other.cols && rows == other.rows && pellets == other.pellets &&
               playerX == other.playerX && playerY == other.playerY && direction == other.direction;
    }
};

// The map rows of an input, following the same rule as the parser: the
// first non-empty line without a '=' starts the map.
std::vector<std::string> mapRowsOf(const std::vector<std::string>& lines) {
    std::vector<std::string> rows;
    bool sawMap = false;
    for (const std::string& raw : lines) {
        std::string line = raw;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const std::string trimmed = trim(line);
        if (!sawMap) {
            if (trimmed.empty()) {
                continue;
            }
            if (trimmed.find('=') != std::string::npos) {
                continue;
            }
            sawMap = true;
        }
        if (trimmed.empty()) {
            continue;
        }
        rows.push_back(line);
    }
    return rows;
}

// Checks the promise. Returns true when the input behaved.
bool checkCase(const std::vector<std::string>& input, std::string* problem) {
    pac::Level out;
    std::string error;
    pac::Level::parse(seedLevel(), out, &error);
    const Snapshot before = Snapshot::of(out);

    bool ok = false;
    try {
        ok = pac::Level::parse(input, out, &error);
    } catch (const std::exception& exception) {
        *problem = std::string("parse threw: ") + exception.what();
        return false;
    } catch (...) {
        *problem = "parse threw an unknown exception";
        return false;
    }

    if (!ok) {
        if (error.empty()) {
            *problem = "parse failed without saying why";
            return false;
        }
        if (!(Snapshot::of(out) == before)) {
            *problem = "a failed parse changed the output level";
            return false;
        }
        return true;
    }

    // A level that was accepted has to make sense.
    if (out.cols() <= 0 || out.rows() <= 0) {
        *problem = "parse accepted an empty map";
        return false;
    }
    if (out.playerX() < 0 || out.playerX() >= out.cols() || out.playerY() < 0 || out.playerY() >= out.rows()) {
        *problem = "the player start is outside the map";
        return false;
    }

    int pellets = 0;
    for (int y = 0; y < out.rows(); ++y) {
        for (int x = 0; x < out.cols(); ++x) {
            if (pac::hasPellet(out.tileAt(x, y))) {
                ++pellets;
            }
        }
    }
    if (pellets != out.pelletCount()) {
        *problem = "the pellet count does not match the map";
        return false;
    }

    for (int i = 0; i < pac::Level::kGhostCount; ++i) {
        const pac::GhostSpawn& spawn = out.ghostSpawn(i);
        if (spawn.x < 0 || spawn.x >= out.cols() || spawn.y < 0 || spawn.y >= out.rows()) {
            *problem = "a ghost start is outside the map";
            return false;
        }
        if (spawn.fromMap && out.tileAt(spawn.x, spawn.y) != pac::Tile::Empty) {
            *problem = "the tile under a ghost marker is not empty";
            return false;
        }
    }
    if (out.tileAt(out.playerX(), out.playerY()) != pac::Tile::Empty) {
        *problem = "the tile under the player marker is not empty";
        return false;
    }

    // The configuration has to be usable.
    const pac::LevelConfig& config = out.config();
    if (config.playerSpeed <= 0.0f || config.ghostSpeed <= 0.0f || config.frightenedSpeed <= 0.0f ||
        config.tunnelSpeed <= 0.0f || config.eyesSpeed <= 0.0f || config.frightenedSeconds <= 0.0f) {
        *problem = "a speed is not positive";
        return false;
    }
    if (config.startingLives <= 0) {
        *problem = "the number of lives is not positive";
        return false;
    }
    for (const float duration : config.phaseDurations) {
        if (duration <= 0.0f) {
            *problem = "a mode phase is not positive";
            return false;
        }
    }

    // The level has to agree with the input it came from.
    const std::vector<std::string> rows = mapRowsOf(input);
    if (rows.empty()) {
        *problem = "parse accepted an input without a map";
        return false;
    }
    if (static_cast<int>(rows.size()) != out.rows()) {
        *problem = "the number of map rows does not match the level";
        return false;
    }
    int playerMarkers = 0;
    std::array<bool, pac::Level::kGhostCount> ghostSeen{};
    for (std::size_t y = 0; y < rows.size(); ++y) {
        if (static_cast<int>(rows[y].size()) != out.cols()) {
            *problem = "the width of a map row does not match the level";
            return false;
        }
        for (std::size_t x = 0; x < rows[y].size(); ++x) {
            const char c = rows[y][x];
            if (c == 'P') {
                ++playerMarkers;
                if (out.playerX() != static_cast<int>(x) || out.playerY() != static_cast<int>(y)) {
                    *problem = "the player start does not match the marker";
                    return false;
                }
                continue;
            }
            if (!pac::isGhostMarker(c)) {
                continue;
            }
            const std::size_t index = static_cast<std::size_t>(c - '1');
            if (ghostSeen[index]) {
                *problem = "parse accepted a map with two ghosts of the same kind";
                return false;
            }
            ghostSeen[index] = true;
            const pac::GhostSpawn& spawn = out.ghostSpawn(static_cast<int>(index));
            if (!spawn.fromMap || spawn.x != static_cast<int>(x) || spawn.y != static_cast<int>(y)) {
                *problem = "a ghost start does not match its marker";
                return false;
            }
        }
    }
    if (playerMarkers != 1) {
        *problem = "parse accepted a map without exactly one player marker";
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Generating inputs
// ---------------------------------------------------------------------------

std::string randomRow(pac::Random& random) {
    const int width = random.range(0, 12);
    std::string row;
    row.reserve(static_cast<std::size_t>(width));
    for (int i = 0; i < width; ++i) {
        row.push_back(kMapAlphabet[random.range(0, kMapAlphabetSize - 1)]);
    }
    return row;
}

std::string randomHeaderLine(pac::Random& random) {
    const std::size_t keyCount = sizeof(kHeaderKeys) / sizeof(kHeaderKeys[0]);
    const std::size_t valueCount = sizeof(kHeaderValues) / sizeof(kHeaderValues[0]);
    const std::string key = kHeaderKeys[random.range(0, static_cast<int>(keyCount) - 1)];
    const std::string value = kHeaderValues[random.range(0, static_cast<int>(valueCount) - 1)];
    switch (random.range(0, 4)) {
    case 0:
        return key + " = " + value;
    case 1:
        return key + "=" + value;
    case 2:
        return "  " + key + "  =  " + value + "  ";
    case 3:
        return key;
    default:
        break;
    }
    return "=" + value;
}

// A level built from nothing.
std::vector<std::string> randomInput(pac::Random& random) {
    std::vector<std::string> lines;

    const int headerCount = random.range(0, 4);
    for (int i = 0; i < headerCount; ++i) {
        lines.push_back(randomHeaderLine(random));
    }
    if (random.range(0, 3) > 0) {
        lines.push_back(std::string());
    }

    const int rowCount = random.range(0, 10);
    for (int i = 0; i < rowCount; ++i) {
        lines.push_back(randomRow(random));
    }
    return lines;
}

// A level built by breaking a level that works. Mutations reach the error
// paths that random noise rarely does, because they start from a map that
// is almost right.
std::vector<std::string> mutate(pac::Random& random, const std::vector<std::string>& base) {
    std::vector<std::string> lines = base;
    const int mutations = random.range(1, 4);
    for (int i = 0; i < mutations && !lines.empty(); ++i) {
        const std::size_t index = static_cast<std::size_t>(random.range(0, static_cast<int>(lines.size()) - 1));
        switch (random.range(0, 7)) {
        case 0: {  // change a character
            std::string& line = lines[index];
            if (line.empty()) {
                break;
            }
            const std::size_t column = static_cast<std::size_t>(random.range(0, static_cast<int>(line.size()) - 1));
            line[column] = kMapAlphabet[random.range(0, kMapAlphabetSize - 1)];
            break;
        }
        case 1: {  // drop a line
            lines.erase(lines.begin() + static_cast<std::ptrdiff_t>(index));
            break;
        }
        case 2: {  // add a line
            lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(index), randomRow(random));
            break;
        }
        case 3: {  // repeat a line
            lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(index), lines[index]);
            break;
        }
        case 4: {  // add a header
            lines.insert(lines.begin(), randomHeaderLine(random));
            break;
        }
        case 5: {  // cut a row short
            std::string& line = lines[index];
            if (!line.empty()) {
                line.erase(static_cast<std::size_t>(random.range(0, static_cast<int>(line.size()) - 1)));
            }
            break;
        }
        case 6: {  // swap two lines
            const std::size_t other = static_cast<std::size_t>(random.range(0, static_cast<int>(lines.size()) - 1));
            std::swap(lines[index], lines[other]);
            break;
        }
        default: {  // a file that was written on Windows
            lines[index] += '\r';
            break;
        }
        }
    }
    return lines;
}

// ---------------------------------------------------------------------------
// The corpus
// ---------------------------------------------------------------------------

std::vector<std::string> readLines(const std::string& path) {
    std::vector<std::string> lines;
    std::ifstream stream(path);
    if (!stream) {
        return lines;
    }
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return lines;
}

// The levels that ship with the game: they are valid, so they are the best
// starting point for a mutation.
std::vector<std::vector<std::string>> shippedLevels() {
    std::vector<std::vector<std::string>> levels;
    std::error_code error;
    if (!fs::exists(PACMAN_LEVELS_DIR, error)) {
        return levels;
    }
    std::vector<fs::path> paths;
    for (fs::directory_iterator entry(PACMAN_LEVELS_DIR, error), end; entry != end; entry.increment(error)) {
        if (entry->is_regular_file(error) && entry->path().extension() == ".txt") {
            paths.push_back(entry->path());
        }
    }
    std::sort(paths.begin(), paths.end());
    for (const fs::path& path : paths) {
        std::vector<std::string> lines = readLines(path.string());
        if (!lines.empty()) {
            levels.push_back(std::move(lines));
        }
    }
    return levels;
}

// ---------------------------------------------------------------------------
// Reporting
// ---------------------------------------------------------------------------

void report(const std::vector<std::string>& input, const std::string& problem) {
    std::cerr << "the parser broke its promise: " << problem << "\n";
    std::cerr << "input:\n";
    for (std::size_t i = 0; i < input.size(); ++i) {
        std::cerr << "  " << (i + 1) << "| " << input[i] << "\n";
    }
}

const char* const kUsage =
    "usage: pacman_fuzz [--iterations N] [--seed S] [--corpus DIR] [--stdin]\n"
    "\n"
    "  --iterations N   how many random cases to run (default 20000)\n"
    "  --seed S         the seed of the random cases (default 20240521)\n"
    "  --corpus DIR     also check every .txt file in DIR\n"
    "  --stdin          read cases from stdin, separated by a line with only %%\n";

}  // namespace

int main(int argc, char** argv) {
    int iterations = kDefaultIterations;
    std::uint32_t seed = kDefaultSeed;
    std::string corpus;
    bool readStdin = false;

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--iterations" && i + 1 < argc) {
            iterations = std::atoi(argv[++i]);
        } else if (argument == "--seed" && i + 1 < argc) {
            seed = static_cast<std::uint32_t>(std::strtoul(argv[++i], nullptr, 0));
        } else if (argument == "--corpus" && i + 1 < argc) {
            corpus = argv[++i];
        } else if (argument == "--stdin") {
            readStdin = true;
        } else if (argument == "--help" || argument == "-h") {
            std::cout << kUsage;
            return 0;
        } else {
            std::cerr << "unknown argument: " << argument << "\n" << kUsage;
            return 2;
        }
    }

    int checked = 0;

    // The levels that ship with the game have to work, and they are the
    // best material for a mutation.
    std::vector<std::vector<std::string>> seeds = shippedLevels();
    for (const std::vector<std::string>& level : seeds) {
        std::string problem;
        if (!checkCase(level, &problem)) {
            report(level, problem);
            return 1;
        }
        ++checked;
    }

    if (!corpus.empty()) {
        std::error_code error;
        if (!fs::is_directory(corpus, error)) {
            std::cerr << "not a directory: " << corpus << "\n";
            return 2;
        }
        for (fs::directory_iterator entry(corpus, error), end; entry != end; entry.increment(error)) {
            if (!entry->is_regular_file(error)) {
                continue;
            }
            const std::vector<std::string> lines = readLines(entry->path().string());
            if (lines.empty()) {
                continue;
            }
            std::string problem;
            if (!checkCase(lines, &problem)) {
                report(lines, problem);
                return 1;
            }
            ++checked;
        }
    }

    if (readStdin) {
        std::vector<std::string> current;
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line == "%%") {
                std::string problem;
                if (!checkCase(current, &problem)) {
                    report(current, problem);
                    return 1;
                }
                ++checked;
                current.clear();
                continue;
            }
            current.push_back(line);
        }
        if (!current.empty()) {
            std::string problem;
            if (!checkCase(current, &problem)) {
                report(current, problem);
                return 1;
            }
            ++checked;
        }
    }

    pac::Random random(seed);
    for (int i = 0; i < iterations; ++i) {
        std::vector<std::string> input;
        if (!seeds.empty() && random.range(0, 2) == 0) {
            const std::size_t index = static_cast<std::size_t>(random.range(0, static_cast<int>(seeds.size()) - 1));
            input = mutate(random, seeds[index]);
        } else {
            input = randomInput(random);
        }

        std::string problem;
        if (!checkCase(input, &problem)) {
            report(input, problem);
            std::cerr << "failed after " << checked << " cases\n";
            return 1;
        }
        ++checked;
    }

    std::cout << "pacman_fuzz: " << checked << " cases, no violation\n";
    return 0;
}
