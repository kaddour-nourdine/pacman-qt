#include <string>

#include "Fixtures.hpp"
#include "TestFramework.hpp"
#include "pacman/Level.hpp"

using namespace pac;

namespace {

bool rejects(const std::vector<std::string>& lines) {
    Level level;
    std::string error;
    return !Level::parse(lines, level, &error);
}

}  // namespace

PAC_TEST(Level, parsesGeometryAndMarkers) {
    const Level level = MAKE_LEVEL(pactest::layoutMap());
    CHECK_EQ(level.cols(), 11);
    CHECK_EQ(level.rows(), 7);
    CHECK_EQ(level.pelletCount(), 31);
    CHECK_EQ(level.playerX(), 6);
    CHECK_EQ(level.playerY(), 1);
    CHECK_EQ(level.playerDirection(), Direction::Left);
    CHECK_EQ(level.ghostSpawn(0).x, 2);
    CHECK_EQ(level.ghostSpawn(0).y, 1);
    CHECK_EQ(level.ghostSpawn(1).x, 4);
    CHECK_EQ(level.ghostSpawn(3).x, 7);
    CHECK_EQ(level.ghostSpawn(3).y, 3);
    CHECK(level.ghostSpawn(0).fromMap);
    CHECK_EQ(level.tileAt(2, 1), Tile::Empty);
    CHECK_EQ(level.tileAt(1, 1), Tile::Pellet);
    CHECK_EQ(level.tileAt(3, 3), Tile::PowerPellet);
    CHECK_EQ(level.tileAt(0, 0), Tile::Wall);
    CHECK_EQ(level.tileAt(-1, 0), Tile::Wall);
    CHECK_EQ(level.tileAt(0, 99), Tile::Wall);
}

PAC_TEST(Level, appliesHeaderOptions) {
    const Level level = MAKE_LEVEL("name = Test Level",
                                   "playerSpeed = 7.5",
                                   "ghostSpeed = 4.25",
                                   "frightenedSpeed = 3.0",
                                   "tunnelSpeed = 2.0",
                                   "eyesSpeed = 18.0",
                                   "frightenedSeconds = 9.5",
                                   "startingLives = 2",
                                   "modeSchedule = 1,2,3",
                                   "",
                                   "#####",
                                   "#P..#",
                                   "#####");
    CHECK(level.name() == "Test Level");
    CHECK_NEAR(level.config().playerSpeed, 7.5, 1e-6);
    CHECK_NEAR(level.config().ghostSpeed, 4.25, 1e-6);
    CHECK_NEAR(level.config().frightenedSpeed, 3.0, 1e-6);
    CHECK_NEAR(level.config().tunnelSpeed, 2.0, 1e-6);
    CHECK_NEAR(level.config().eyesSpeed, 18.0, 1e-6);
    CHECK_NEAR(level.config().frightenedSeconds, 9.5, 1e-6);
    CHECK_EQ(level.config().startingLives, 2);
    CHECK_EQ(level.config().phaseDurations.size(), std::size_t(3));
    CHECK_NEAR(level.config().phaseDurations[2], 3.0, 1e-6);
}

PAC_TEST(Level, usesDefaultOptions) {
    const Level level = MAKE_LEVEL("#####", "#P..#", "#####");
    CHECK_NEAR(level.config().playerSpeed, 6.0, 1e-6);
    CHECK_EQ(level.config().startingLives, 3);
    CHECK(level.config().phaseDurations.empty());
}

PAC_TEST(Level, defaultsMissingGhostSpawns) {
    const Level level = MAKE_LEVEL("#####", "#.P1#", "#####");
    CHECK(!level.ghostSpawn(1).fromMap);
    CHECK(level.ghostSpawn(0).fromMap);
    CHECK_EQ(level.ghostSpawn(2).x, 1);
    CHECK_EQ(level.ghostSpawn(2).y, 1);
    CHECK_EQ(level.ghostSpawn(3).x, 1);
    CHECK_EQ(level.ghostSpawn(3).y, 1);
}

PAC_TEST(Level, rejectsRaggedRows) {
    CHECK(rejects({"#####", "#P..#", "####"}));
}

PAC_TEST(Level, rejectsMissingPlayer) {
    CHECK(rejects({"#####", "#...#", "#####"}));
}

PAC_TEST(Level, rejectsDuplicatePlayer) {
    CHECK(rejects({"#####", "#P.P#", "#####"}));
}

PAC_TEST(Level, rejectsUnknownOption) {
    CHECK(rejects({"nope = 1", "#####", "#P..#", "#####"}));
}

PAC_TEST(Level, rejectsInvalidNumber) {
    CHECK(rejects({"playerSpeed = fast", "#####", "#P..#", "#####"}));
    CHECK(rejects({"startingLives = zero", "#####", "#P..#", "#####"}));
    CHECK(rejects({"modeSchedule = 1,x", "#####", "#P..#", "#####"}));
}

PAC_TEST(Level, rejectsInvalidMapCharacter) {
    CHECK(rejects({"#####", "#P.$.#", "#####"}));
}

PAC_TEST(Level, rejectsBlankLineInsideMap) {
    CHECK(rejects({"#####", "#P..#", "", "#...#", "#####"}));
}

PAC_TEST(Level, rejectsEmptyLevel) {
    CHECK(rejects({"name = Nothing"}));
}

PAC_TEST(Level, loadsLevelFile) {
    Level level;
    std::string error;
    const std::string path = std::string(PACMAN_LEVELS_DIR) + "/level1.txt";
    CHECK(Level::loadFile(path, level, &error));
    if (!level.isValid()) {
        pactest::reportFailure(__FILE__, __LINE__, "level1.txt failed to load: " + error);
        return;
    }
    CHECK_EQ(level.cols(), 21);
    CHECK_EQ(level.rows(), 21);
    CHECK_EQ(level.pelletCount(), 170);
    CHECK_EQ(level.playerX(), 10);
    CHECK_EQ(level.playerY(), 1);
    CHECK_EQ(level.tileAt(0, 10), Tile::Tunnel);
    CHECK_EQ(level.tileAt(20, 10), Tile::Tunnel);
    CHECK_EQ(level.tileAt(10, 6), Tile::Door);
    CHECK_NEAR(level.config().frightenedSeconds, 6.0, 1e-6);
    CHECK_EQ(level.config().phaseDurations.size(), std::size_t(8));
}

PAC_TEST(Level, reportsMissingFile) {
    Level level;
    std::string error;
    CHECK(!Level::loadFile("/does/not/exist.txt", level, &error));
    CHECK(!error.empty());
}