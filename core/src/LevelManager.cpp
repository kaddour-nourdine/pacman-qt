#include "pacman/LevelManager.hpp"

#include <algorithm>
#include <filesystem>
#include <system_error>
#include <utility>

namespace pac {

namespace {

bool fail(std::string* error, const std::string& message) {
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

}  // namespace

LevelManager::LevelManager(std::vector<Level> levels) : levels_(std::move(levels)) {
}

bool LevelManager::add(Level level) {
    if (!level.isValid()) {
        return false;
    }
    levels_.push_back(std::move(level));
    return true;
}

bool LevelManager::loadFile(const std::string& path, std::string* error) {
    Level level;
    if (!Level::loadFile(path, level, error)) {
        return false;
    }
    levels_.push_back(std::move(level));
    return true;
}

bool LevelManager::loadDirectory(const std::string& directory, std::string* error) {
    namespace fs = std::filesystem;

    std::error_code ec;
    const fs::path dir(directory);
    if (!fs::is_directory(dir, ec)) {
        return fail(error, "not a directory: '" + directory + "'");
    }

    std::vector<fs::path> files;
    fs::directory_iterator it(dir, ec);
    if (ec) {
        return fail(error, "cannot list directory '" + directory + "': " + ec.message());
    }
    const fs::directory_iterator end;
    for (; it != end; it.increment(ec)) {
        if (ec) {
            return fail(error, "cannot list directory '" + directory + "': " + ec.message());
        }
        const fs::path entry = it->path();
        if (entry.extension() != ".txt") {
            continue;
        }
        if (fs::is_regular_file(entry, ec)) {
            files.push_back(entry);
        }
        if (ec) {
            return fail(error, "cannot inspect '" + entry.string() + "': " + ec.message());
        }
    }

    if (files.empty()) {
        return fail(error, "no level files ('*.txt') in '" + directory + "'");
    }

    std::sort(files.begin(), files.end(), [](const fs::path& a, const fs::path& b) {
        return a.filename().string() < b.filename().string();
    });

    std::vector<Level> loaded;
    loaded.reserve(files.size());
    for (const fs::path& file : files) {
        Level level;
        std::string levelError;
        if (!Level::loadFile(file.string(), level, &levelError)) {
            return fail(error, file.filename().string() + ": " + levelError);
        }
        loaded.push_back(std::move(level));
    }

    for (Level& level : loaded) {
        levels_.push_back(std::move(level));
    }
    return true;
}

const Level& LevelManager::levelFor(int levelNumber) const {
    static const Level empty;
    if (levels_.empty()) {
        return empty;
    }
    const std::size_t number = static_cast<std::size_t>(levelNumber < 1 ? 1 : levelNumber);
    return levels_[(number - 1) % levels_.size()];
}

const std::string& LevelManager::nameFor(int levelNumber) const {
    static const std::string noName;
    if (levels_.empty()) {
        return noName;
    }
    return levelFor(levelNumber).name();
}

}
