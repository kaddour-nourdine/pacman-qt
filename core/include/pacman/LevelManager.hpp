#pragma once

#include <string>
#include <vector>

#include "pacman/Level.hpp"

namespace pac {

class LevelManager {
public:
    LevelManager() = default;
    explicit LevelManager(std::vector<Level> levels);

    bool add(Level level);
    bool loadFile(const std::string& path, std::string* error);
    bool loadDirectory(const std::string& directory, std::string* error);
    void clear() { levels_.clear(); }

    int levelCount() const { return static_cast<int>(levels_.size()); }
    bool isEmpty() const { return levels_.empty(); }
    const std::vector<Level>& levels() const { return levels_; }

    const Level& levelFor(int levelNumber) const;
    const std::string& nameFor(int levelNumber) const;

private:
    std::vector<Level> levels_;
};

}
