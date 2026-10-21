#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include "Fixtures.hpp"
#include "TestFramework.hpp"
#include "pacman/LevelManager.hpp"

using namespace pac;

namespace {

namespace fs = std::filesystem;

Level roomLevel(const std::vector<std::string>& rows) {
    return pactest::makeLevel(rows, __FILE__, __LINE__);
}

const char* kAlphaLevel =
    "name = Alpha\n"
    "#####\n"
    "#P..#\n"
    "#o..#\n"
    "#####\n";

const char* kBetaLevel =
    "name = Beta\n"
    "#####\n"
    "#P..#\n"
    "#.o.#\n"
    "#####\n";

const char* kBrokenLevel =
    "name = Broken\n"
    "#####\n"
    "#P.x#\n"
    "#####\n";

class TempLevelsDir {
public:
    explicit TempLevelsDir(const char* name)
        : path_(fs::temp_directory_path() / ("pacman_levels_" + std::string(name))) {
        std::error_code ec;
        fs::remove_all(path_, ec);
        fs::create_directories(path_, ec);
    }

    ~TempLevelsDir() {
        std::error_code ec;
        fs::remove_all(path_, ec);
    }

    TempLevelsDir(const TempLevelsDir&) = delete;
    TempLevelsDir& operator=(const TempLevelsDir&) = delete;

    std::string string() const { return path_.string(); }

    void write(const std::string& fileName, const char* content) const {
        std::ofstream stream(path_ / fileName);
        stream << content;
    }

private:
    fs::path path_;
};

}  // namespace

PAC_TEST(LevelManager, startsEmpty) {
    const LevelManager manager;
    CHECK(manager.isEmpty());
    CHECK_EQ(manager.levelCount(), 0);
    CHECK_EQ(manager.levels().size(), std::size_t(0));
    CHECK(!manager.levelFor(1).isValid());
    CHECK(manager.nameFor(1).empty());
    CHECK(manager.nameFor(0).empty());
}

PAC_TEST(LevelManager, picksLevelsByNumberAndWraps) {
    LevelManager manager;
    CHECK(manager.add(roomLevel(pactest::firstRoomMap())));
    CHECK(manager.add(roomLevel(pactest::secondRoomMap())));
    CHECK_EQ(manager.levelCount(), 2);

    CHECK(manager.levelFor(1).name() == "First Room");
    CHECK(manager.levelFor(2).name() == "Second Room");
    CHECK(manager.levelFor(3).name() == "First Room");
    CHECK(manager.levelFor(4).name() == "Second Room");
    CHECK(manager.levelFor(0).name() == "First Room");
    CHECK(manager.levelFor(-7).name() == "First Room");

    CHECK(manager.nameFor(1) == "First Room");
    CHECK(manager.nameFor(2) == "Second Room");
    CHECK(manager.nameFor(3) == "First Room");

    CHECK_EQ(manager.levelFor(1).pelletCount(), 15);
    CHECK_EQ(manager.levelFor(2).pelletCount(), 12);
}

PAC_TEST(LevelManager, rejectsInvalidLevel) {
    LevelManager manager;
    CHECK(!manager.add(Level()));
    CHECK(manager.isEmpty());
    CHECK_EQ(manager.levelCount(), 0);
}

PAC_TEST(LevelManager, clearEmptiesTheList) {
    LevelManager manager;
    CHECK(manager.add(roomLevel(pactest::firstRoomMap())));
    CHECK(!manager.isEmpty());

    manager.clear();
    CHECK(manager.isEmpty());
    CHECK(manager.nameFor(1).empty());
}

PAC_TEST(LevelManager, loadsSingleLevelFile) {
    LevelManager manager;
    std::string error;
    CHECK(manager.loadFile(std::string(PACMAN_LEVELS_DIR) + "/level1.txt", &error));
    CHECK_EQ(manager.levelCount(), 1);
    CHECK(manager.nameFor(1) == "Level 1");
    CHECK_EQ(manager.levelFor(1).pelletCount(), 178);
    CHECK_EQ(manager.levelFor(2).pelletCount(), 178);
}

PAC_TEST(LevelManager, reportsMissingLevelFile) {
    LevelManager manager;
    std::string error;
    CHECK(!manager.loadFile("/does/not/exist.txt", &error));
    CHECK(!error.empty());
    CHECK(manager.isEmpty());
}

PAC_TEST(LevelManager, loadsDirectorySortedByFileName) {
    const TempLevelsDir dir("sorted");
    dir.write("02_beta.txt", kBetaLevel);
    dir.write("01_alpha.txt", kAlphaLevel);
    dir.write("notes.md", "not a level\n");

    LevelManager manager;
    std::string error;
    CHECK(manager.loadDirectory(dir.string(), &error));
    CHECK_EQ(manager.levelCount(), 2);
    CHECK(manager.nameFor(1) == "Alpha");
    CHECK(manager.nameFor(2) == "Beta");
    CHECK(manager.nameFor(3) == "Alpha");
}

PAC_TEST(LevelManager, appendsDirectoryToExistingLevels) {
    const TempLevelsDir dir("append");
    dir.write("01_alpha.txt", kAlphaLevel);

    LevelManager manager;
    std::string error;
    CHECK(manager.add(roomLevel(pactest::firstRoomMap())));
    CHECK(manager.loadDirectory(dir.string(), &error));
    CHECK_EQ(manager.levelCount(), 2);
    CHECK(manager.nameFor(1) == "First Room");
    CHECK(manager.nameFor(2) == "Alpha");
}

PAC_TEST(LevelManager, rejectsDirectoryWithBrokenLevel) {
    const TempLevelsDir dir("broken");
    dir.write("01_alpha.txt", kAlphaLevel);
    dir.write("02_broken.txt", kBrokenLevel);

    LevelManager manager;
    std::string error;
    CHECK(!manager.loadDirectory(dir.string(), &error));
    CHECK(!error.empty());
    CHECK(error.find("02_broken.txt") != std::string::npos);
    CHECK(manager.isEmpty());
}

PAC_TEST(LevelManager, reportsMissingDirectory) {
    LevelManager manager;
    std::string error;
    CHECK(!manager.loadDirectory("/does/not/exist", &error));
    CHECK(!error.empty());
    CHECK(manager.isEmpty());
}

PAC_TEST(LevelManager, reportsDirectoryWithoutLevels) {
    const TempLevelsDir dir("empty");

    LevelManager manager;
    std::string error;
    CHECK(!manager.loadDirectory(dir.string(), &error));
    CHECK(!error.empty());
    CHECK(manager.isEmpty());
}

PAC_TEST(LevelManager, reportsFailuresWithoutAnErrorSlot) {
    // The caller may not care about the message; the load still fails.
    LevelManager manager;
    CHECK(!manager.loadDirectory("/does/not/exist", nullptr));
    CHECK(manager.isEmpty());
}

PAC_TEST(LevelManager, skipsFoldersThatLookLikeLevels) {
    const TempLevelsDir dir("folder");
    dir.write("01_alpha.txt", kAlphaLevel);
    std::error_code ec;
    fs::create_directory(fs::path(dir.string()) / "02_beta.txt", ec);

    LevelManager manager;
    std::string error;
    CHECK(manager.loadDirectory(dir.string(), &error));
    CHECK_EQ(manager.levelCount(), 1);
    CHECK(manager.nameFor(1) == "Alpha");
}

PAC_TEST(LevelManager, keepsTheEmptyLevelForLaterCalls) {
    const LevelManager manager;
    CHECK(!manager.levelFor(1).isValid());
    CHECK(!manager.levelFor(2).isValid());
    CHECK(manager.nameFor(1).empty());
    CHECK(manager.nameFor(2).empty());
}
