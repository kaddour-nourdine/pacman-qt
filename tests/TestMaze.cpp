#include "Fixtures.hpp"
#include "TestFramework.hpp"
#include "pacman/Maze.hpp"

using namespace pac;

PAC_TEST(Maze, reportsGeometry) {
    const Maze maze(pactest::makeLevel(pactest::layoutMap(), __FILE__, __LINE__));
    CHECK_EQ(maze.cols(), 11);
    CHECK_EQ(maze.rows(), 7);
    CHECK_EQ(maze.pelletsTotal(), 31);
    CHECK_EQ(maze.pelletsRemaining(), 31);
    CHECK_EQ(maze.pelletsEaten(), 0);
    CHECK(!maze.allPelletsEaten());
}

PAC_TEST(Maze, findsTheTileUnderAPosition) {
    const Maze maze(pactest::makeLevel(pactest::layoutMap(), __FILE__, __LINE__));
    CHECK_EQ(maze.tileAt(Vec2{1.5f, 1.5f}), Tile::Pellet);
    CHECK_EQ(maze.tileAt(Vec2{0.5f, 0.5f}), Tile::Wall);
    CHECK_EQ(maze.tileAt(Vec2{-0.5f, 1.5f}), Tile::Wall);
}

PAC_TEST(Maze, treatsOutsideAsWall) {
    const Maze maze(pactest::makeLevel(pactest::layoutMap(), __FILE__, __LINE__));
    CHECK_EQ(maze.tileAt(-1, 1), Tile::Wall);
    CHECK_EQ(maze.tileAt(0, 1), Tile::Wall);
    CHECK_EQ(maze.tileAt(1, -1), Tile::Wall);
    CHECK_EQ(maze.tileAt(1, 7), Tile::Wall);
    CHECK(!maze.canEnter(0, 1, false));
    CHECK(!maze.canEnter(0, 1, true));
}

PAC_TEST(Maze, pelletsAreConsumedOnce) {
    Maze maze(pactest::makeLevel(pactest::layoutMap(), __FILE__, __LINE__));
    CHECK_EQ(maze.takePellet(1, 1), Tile::Pellet);
    CHECK_EQ(maze.pelletsRemaining(), 30);
    CHECK_EQ(maze.takePellet(1, 1), Tile::Empty);
    CHECK_EQ(maze.pelletsRemaining(), 30);
    CHECK_EQ(maze.takePellet(3, 3), Tile::PowerPellet);
    CHECK_EQ(maze.takePellet(1, 1), Tile::Empty);
    CHECK_EQ(maze.pelletsEaten(), 2);
    CHECK(!maze.isPellet(1, 1));
}

PAC_TEST(Maze, detectsLevelCompletion) {
    Maze maze(pactest::makeLevel(pactest::layoutMap(), __FILE__, __LINE__));
    for (int y = 0; y < maze.rows(); ++y) {
        for (int x = 0; x < maze.cols(); ++x) {
            maze.takePellet(x, y);
        }
    }
    CHECK_EQ(maze.pelletsRemaining(), 0);
    CHECK(maze.allPelletsEaten());
}

PAC_TEST(Maze, resetsPellets) {
    Maze maze(pactest::makeLevel(pactest::layoutMap(), __FILE__, __LINE__));
    for (int y = 0; y < maze.rows(); ++y) {
        for (int x = 0; x < maze.cols(); ++x) {
            maze.takePellet(x, y);
        }
    }
    maze.resetPellets();
    CHECK_EQ(maze.pelletsRemaining(), 31);
    CHECK_EQ(maze.takePellet(1, 1), Tile::Pellet);
}

PAC_TEST(Maze, doorsBlockOnlyNonGhosts) {
    const Maze maze(pactest::makeLevel(pactest::houseMap(), __FILE__, __LINE__));
    CHECK_EQ(maze.tileAt(5, 3), Tile::Door);
    CHECK(!maze.canEnter(5, 3, false));
    CHECK(maze.canEnter(5, 3, true));
    CHECK_EQ(maze.stepToward(5, 1, 3, 4, false), Direction::None);
    CHECK_EQ(maze.stepToward(5, 1, 3, 4, true), Direction::Down);
}

PAC_TEST(Maze, wrapsThroughTunnels) {
    const Maze maze(pactest::makeLevel(pactest::ringMap(), __FILE__, __LINE__));
    CHECK_EQ(maze.wrapX(-1, 1), 10);
    CHECK_EQ(maze.wrapX(11, 1), 0);
    CHECK_EQ(maze.wrapX(-1, 0), -1);
    CHECK_EQ(maze.wrapX(11, 0), 11);
    CHECK(maze.canEnter(-1, 1, false));
    CHECK(nearlyEquals(maze.wrapPosition(Vec2{-0.5f, 1.5f}), Vec2{10.5f, 1.5f}));
    CHECK(nearlyEquals(maze.wrapPosition(Vec2{11.5f, 1.5f}), Vec2{0.5f, 1.5f}));
    CHECK(nearlyEquals(maze.wrapPosition(Vec2{4.5f, 1.5f}), Vec2{4.5f, 1.5f}));
    CHECK_EQ(maze.tileXOf(Vec2{-0.5f, 1.5f}), 10);
    CHECK_EQ(maze.tileXOf(Vec2{10.75f, 1.5f}), 10);
}

PAC_TEST(Maze, tunnelTilesHoldNoPellets) {
    Maze maze(pactest::makeLevel(pactest::ringMap(), __FILE__, __LINE__));
    CHECK_EQ(maze.takePellet(0, 1), Tile::Empty);
    CHECK_EQ(maze.pelletsRemaining(), 8);
    CHECK_EQ(maze.takePellet(1, 1), Tile::PowerPellet);
    CHECK_EQ(maze.pelletsRemaining(), 7);
}

PAC_TEST(Maze, findsShortestPath) {
    const Maze maze(pactest::makeLevel(pactest::layoutMap(), __FILE__, __LINE__));
    CHECK_EQ(maze.stepToward(1, 1, 3, 1, false), Direction::Right);
    CHECK_EQ(maze.stepToward(3, 1, 1, 1, false), Direction::Left);
    CHECK_EQ(maze.stepToward(1, 1, 1, 3, false), Direction::Down);
    CHECK_EQ(maze.stepToward(1, 3, 1, 1, false), Direction::Up);
    CHECK_EQ(maze.stepToward(1, 1, 1, 1, false), Direction::None);
}

PAC_TEST(Maze, findsPathAcrossTunnel) {
    const Maze maze(pactest::makeLevel(pactest::ringMap(), __FILE__, __LINE__));
    CHECK_EQ(maze.stepToward(1, 1, 9, 1, false), Direction::Left);
    CHECK_EQ(maze.stepToward(9, 1, 1, 1, false), Direction::Right);
    CHECK_EQ(maze.stepToward(0, 1, 0, 1, false), Direction::None);
}

// --- The corners of the map ------------------------------------------------
//
// The path search and the pellet lookup have to behave when they are asked
// for a tile that is not on the map.

PAC_TEST(Maze, treatsTheRightEdgeAsWall) {
    const Maze maze(pactest::makeLevel(pactest::layoutMap(), __FILE__, __LINE__));
    CHECK_EQ(maze.tileAt(11, 1), Tile::Wall);
    CHECK_EQ(maze.tileAt(-1, 1), Tile::Wall);
}

PAC_TEST(Maze, ignoresPelletsOutsideTheMap) {
    Maze maze(pactest::makeLevel(pactest::layoutMap(), __FILE__, __LINE__));
    CHECK_EQ(maze.takePellet(-1, 1), Tile::Empty);
    CHECK_EQ(maze.takePellet(11, 1), Tile::Empty);
    CHECK_EQ(maze.takePellet(1, -1), Tile::Empty);
    CHECK_EQ(maze.takePellet(1, 7), Tile::Empty);
    CHECK_EQ(maze.pelletsRemaining(), 31);
    CHECK_EQ(maze.pelletsEaten(), 0);
}

PAC_TEST(Maze, hasNoPathWithoutAMap) {
    const Maze maze{pac::Level{}};
    CHECK(!maze.level().isValid());
    CHECK_EQ(maze.stepToward(0, 0, 1, 1, false), Direction::None);
}

PAC_TEST(Maze, wrapsNothingWithoutAMap) {
    const Maze maze{pac::Level{}};
    CHECK_EQ(maze.wrapX(3, 0), 3);
    CHECK_EQ(maze.wrapX(-3, 0), -3);
    CHECK_EQ(maze.tileXOf(Vec2{1.5f, 1.5f}), 1);
}

PAC_TEST(Maze, refusesToStepOutsideTheMap) {
    const Maze maze(pactest::makeLevel(pactest::layoutMap(), __FILE__, __LINE__));
    CHECK_EQ(maze.stepToward(-5, 1, 3, 1, false), Direction::None);
    CHECK_EQ(maze.stepToward(1, -1, 3, 1, false), Direction::None);
    CHECK_EQ(maze.stepToward(1, 7, 3, 1, false), Direction::None);
    CHECK_EQ(maze.stepToward(1, 1, -5, 1, false), Direction::None);
    CHECK_EQ(maze.stepToward(1, 1, 3, -1, false), Direction::None);
    CHECK_EQ(maze.stepToward(1, 1, 3, 7, false), Direction::None);

    // Outside the map in both directions at once.
    CHECK_EQ(maze.stepToward(-5, -1, 3, 1, false), Direction::None);
    CHECK_EQ(maze.stepToward(-5, 7, 3, 1, false), Direction::None);
}

PAC_TEST(Maze, searchesAlongTheEdgesOfTheMap) {
    const Maze maze(pactest::makeLevel(pactest::edgeMap(), __FILE__, __LINE__));
    CHECK_EQ(maze.stepToward(1, 1, 3, 1, false), Direction::Right);
    CHECK_EQ(maze.stepToward(1, 1, 3, 2, false), Direction::Right);
    CHECK_EQ(maze.stepToward(0, 0, 3, 0, false), Direction::Right);
}