#include "pacman/Level.hpp"

#include <cstdlib>
#include <fstream>
#include <utility>

namespace pac {

namespace {

bool fail(std::string* error, const std::string& message) {
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

std::string trim(const std::string& text) {
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return std::string();
    }
    const std::size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

bool parseFloat(const std::string& text, float& out) {
    if (text.empty()) {
        return false;
    }
    char* end = nullptr;
    const float value = std::strtof(text.c_str(), &end);
    if (end == text.c_str() || *end != '\0') {
        return false;
    }
    out = value;
    return true;
}

bool parseInt(const std::string& text, int& out) {
    if (text.empty()) {
        return false;
    }
    char* end = nullptr;
    const long value = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0') {
        return false;
    }
    out = static_cast<int>(value);
    return true;
}

std::vector<std::string> splitCommas(const std::string& text) {
    std::vector<std::string> parts;
    std::string current;
    for (const char c : text) {
        if (c == ',') {
            parts.push_back(trim(current));
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    parts.push_back(trim(current));
    return parts;
}

bool applyHeaderEntry(const std::string& key, const std::string& value, LevelConfig& config, std::string& name,
                      std::string* error) {
    const std::string bad = "invalid value for '" + key + "': '" + value + "'";

    if (key == "name") {
        if (value.empty()) {
            return fail(error, "empty value for 'name'");
        }
        name = value;
        return true;
    }

    float speed = 0.0f;
    if (key == "playerSpeed") {
        if (!parseFloat(value, speed) || speed <= 0.0f) {
            return fail(error, bad);
        }
        config.playerSpeed = speed;
        return true;
    }
    if (key == "ghostSpeed") {
        if (!parseFloat(value, speed) || speed <= 0.0f) {
            return fail(error, bad);
        }
        config.ghostSpeed = speed;
        return true;
    }
    if (key == "frightenedSpeed") {
        if (!parseFloat(value, speed) || speed <= 0.0f) {
            return fail(error, bad);
        }
        config.frightenedSpeed = speed;
        return true;
    }
    if (key == "tunnelSpeed") {
        if (!parseFloat(value, speed) || speed <= 0.0f) {
            return fail(error, bad);
        }
        config.tunnelSpeed = speed;
        return true;
    }
    if (key == "eyesSpeed") {
        if (!parseFloat(value, speed) || speed <= 0.0f) {
            return fail(error, bad);
        }
        config.eyesSpeed = speed;
        return true;
    }
    if (key == "frightenedSeconds") {
        if (!parseFloat(value, speed) || speed <= 0.0f) {
            return fail(error, bad);
        }
        config.frightenedSeconds = speed;
        return true;
    }
    if (key == "startingLives") {
        int lives = 0;
        if (!parseInt(value, lives) || lives <= 0) {
            return fail(error, bad);
        }
        config.startingLives = lives;
        return true;
    }
    if (key == "modeSchedule") {
        const std::vector<std::string> parts = splitCommas(value);
        if (parts.empty()) {
            return fail(error, bad);
        }
        std::vector<float> durations;
        durations.reserve(parts.size());
        for (const std::string& part : parts) {
            float duration = 0.0f;
            if (!parseFloat(part, duration) || duration <= 0.0f) {
                return fail(error, bad);
            }
            durations.push_back(duration);
        }
        config.phaseDurations = std::move(durations);
        return true;
    }

    return fail(error, "unknown level option '" + key + "'");
}

}  // namespace

bool Level::parse(const std::vector<std::string>& lines, Level& out, std::string* error) {
    Level level;
    std::vector<std::string> mapRows;
    bool sawMap = false;
    bool sawBlankAfterMap = false;

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
            const std::size_t separator = trimmed.find('=');
            if (separator != std::string::npos) {
                const std::string key = trim(trimmed.substr(0, separator));
                const std::string value = trim(trimmed.substr(separator + 1));
                if (!applyHeaderEntry(key, value, level.config_, level.name_, error)) {
                    return false;
                }
                continue;
            }
            sawMap = true;
        }

        if (trimmed.empty()) {
            sawBlankAfterMap = true;
            continue;
        }
        if (sawBlankAfterMap && !mapRows.empty()) {
            return fail(error, "blank line inside the map body");
        }
        mapRows.push_back(line);
    }

    if (mapRows.empty()) {
        return fail(error, "level has no map body");
    }

    const int cols = static_cast<int>(mapRows.front().size());
    if (cols <= 0) {
        return fail(error, "map rows must not be empty");
    }
    for (const std::string& row : mapRows) {
        if (static_cast<int>(row.size()) != cols) {
            return fail(error, "all map rows must have the same width");
        }
        for (const char c : row) {
            if (!isMapChar(c)) {
                return fail(error, std::string("invalid map character '") + c + "'");
            }
        }
    }

    const int rows = static_cast<int>(mapRows.size());
    level.cols_ = cols;
    level.rows_ = rows;
    level.tiles_.assign(static_cast<std::size_t>(cols) * static_cast<std::size_t>(rows), Tile::Empty);

    std::array<GhostSpawn, kGhostCount> ghosts{};
    std::array<bool, kGhostCount> ghostSeen{};
    int playerMarkers = 0;

    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            const char c = mapRows[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
            if (c == 'P') {
                ++playerMarkers;
                level.playerX_ = x;
                level.playerY_ = y;
                level.playerDirection_ = Direction::Left;
                continue;
            }
            if (isGhostMarker(c)) {
                const int index = c - '1';
                GhostSpawn spawn;
                spawn.x = x;
                spawn.y = y;
                spawn.direction = Direction::Left;
                spawn.fromMap = true;
                ghosts[static_cast<std::size_t>(index)] = spawn;
                ghostSeen[static_cast<std::size_t>(index)] = true;
                continue;
            }
            const Tile tile = charToTile(c);
            level.tiles_[static_cast<std::size_t>(y) * static_cast<std::size_t>(cols) + static_cast<std::size_t>(x)] = tile;
            if (hasPellet(tile)) {
                ++level.pelletCount_;
            }
        }
    }

    if (playerMarkers == 0) {
        return fail(error, "map has no player start marker 'P'");
    }
    if (playerMarkers > 1) {
        return fail(error, "map has more than one player start marker 'P'");
    }

    for (int i = 0; i < kGhostCount; ++i) {
        const std::size_t index = static_cast<std::size_t>(i);
        if (ghostSeen[index]) {
            level.ghosts_[index] = ghosts[index];
            continue;
        }
        bool placed = false;
        for (int y = 0; y < rows && !placed; ++y) {
            for (int x = 0; x < cols && !placed; ++x) {
                const Tile tile = level.tileAt(x, y);
                if (!isWalkable(tile)) {
                    continue;
                }
                if (x == level.playerX_ && y == level.playerY_) {
                    continue;
                }
                GhostSpawn spawn;
                spawn.x = x;
                spawn.y = y;
                spawn.direction = Direction::Left;
                spawn.fromMap = false;
                level.ghosts_[index] = spawn;
                placed = true;
            }
        }
        if (!placed) {
            return fail(error, "map has no free tile left for ghost spawn");
        }
    }

    out = std::move(level);
    return true;
}

bool Level::loadFile(const std::string& path, Level& out, std::string* error) {
    std::ifstream stream(path);
    if (!stream) {
        return fail(error, "cannot open level file '" + path + "'");
    }
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return parse(lines, out, error);
}

}