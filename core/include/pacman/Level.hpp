#pragma once

#include <array>
#include <string>
#include <vector>

#include "pacman/Direction.hpp"
#include "pacman/Tile.hpp"

namespace pac {

struct LevelConfig {
    float playerSpeed = 6.0f;
    float ghostSpeed = 5.0f;
    float frightenedSpeed = 3.5f;
    float tunnelSpeed = 4.0f;
    float eyesSpeed = 16.0f;
    float frightenedSeconds = 6.0f;
    int startingLives = 3;
    std::vector<float> phaseDurations;
};

struct GhostSpawn {
    int x = 0;
    int y = 0;
    Direction direction = Direction::Left;
    bool fromMap = false;
};

class Level {
public:
    static constexpr int kGhostCount = 4;

    static bool parse(const std::vector<std::string>& lines, Level& out, std::string* error);
    static bool loadFile(const std::string& path, Level& out, std::string* error);

    const std::string& name() const { return name_; }
    int cols() const { return cols_; }
    int rows() const { return rows_; }
    int pelletCount() const { return pelletCount_; }
    bool isValid() const { return cols_ > 0 && rows_ > 0; }

    Tile tileAt(int x, int y) const {
        if (x < 0 || x >= cols_ || y < 0 || y >= rows_) {
            return Tile::Wall;
        }
        return tiles_[static_cast<std::size_t>(y) * static_cast<std::size_t>(cols_) + static_cast<std::size_t>(x)];
    }

    int playerX() const { return playerX_; }
    int playerY() const { return playerY_; }
    Direction playerDirection() const { return playerDirection_; }

    const GhostSpawn& ghostSpawn(int index) const {
        static const GhostSpawn fallback;
        if (index < 0 || index >= kGhostCount) {
            return fallback;
        }
        return ghosts_[static_cast<std::size_t>(index)];
    }

    const LevelConfig& config() const { return config_; }

private:
    std::string name_ = "untitled";
    int cols_ = 0;
    int rows_ = 0;
    std::vector<Tile> tiles_;
    int playerX_ = 0;
    int playerY_ = 0;
    Direction playerDirection_ = Direction::Left;
    std::array<GhostSpawn, kGhostCount> ghosts_{};
    LevelConfig config_;
    int pelletCount_ = 0;
};

}