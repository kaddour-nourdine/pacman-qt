#include <string>

#include "Fixtures.hpp"
#include "TestFramework.hpp"
#include "pacman/Ghost.hpp"

using namespace pac;

namespace {

Maze layout() {
    return Maze(pactest::makeLevel(pactest::layoutMap(), __FILE__, __LINE__));
}

Maze house() {
    return Maze(pactest::makeLevel(pactest::houseMap(), __FILE__, __LINE__));
}

Maze junction() {
    return Maze(pactest::makeLevel(pactest::junctionMap(), __FILE__, __LINE__));
}

}  // namespace

PAC_TEST(Ghost, spawnsAtLevelPosition) {
    Maze maze = layout();
    Random random(1234);
    const Ghost ghost(maze, GhostId::Blinky, 4, 3, Direction::Left, &random);
    CHECK_EQ(ghost.tileX(), 4);
    CHECK_EQ(ghost.tileY(), 3);
    CHECK_EQ(ghost.mode(), GhostMode::Scatter);
    CHECK(!ghost.isEdible());
    CHECK(!ghost.isEyes());
}

PAC_TEST(Ghost, pointValues) {
    Maze maze = layout();
    Random random(1234);
    CHECK_EQ(Ghost(maze, GhostId::Blinky, 4, 3, Direction::Left, &random).pointsValue(), 200);
    CHECK_EQ(Ghost(maze, GhostId::Pinky, 4, 3, Direction::Left, &random).pointsValue(), 400);
    CHECK_EQ(Ghost(maze, GhostId::Inky, 4, 3, Direction::Left, &random).pointsValue(), 800);
    CHECK_EQ(Ghost(maze, GhostId::Clyde, 4, 3, Direction::Left, &random).pointsValue(), 1600);
}

PAC_TEST(Ghost, scatterTargets) {
    Maze maze = layout();
    Random random(99);
    const Ghost blinky(maze, GhostId::Blinky, 4, 3, Direction::Left, &random);
    const Ghost pinky(maze, GhostId::Pinky, 4, 3, Direction::Left, &random);
    const Ghost inky(maze, GhostId::Inky, 4, 3, Direction::Left, &random);
    const Ghost clyde(maze, GhostId::Clyde, 4, 3, Direction::Left, &random);

    CHECK(nearlyEquals(blinky.scatterCorner(), tileCenterOf(10, 0)));
    CHECK(nearlyEquals(pinky.scatterCorner(), tileCenterOf(0, 0)));
    CHECK(nearlyEquals(inky.scatterCorner(), tileCenterOf(10, 6)));
    CHECK(nearlyEquals(clyde.scatterCorner(), tileCenterOf(0, 6)));
    CHECK_EQ(blinky.mode(), GhostMode::Scatter);
    CHECK(nearlyEquals(blinky.target(), tileCenterOf(10, 0)));
}

PAC_TEST(Ghost, blinkyTargetsPlayer) {
    Maze maze = layout();
    Random random(7);
    Ghost ghost(maze, GhostId::Blinky, 4, 3, Direction::Left, &random);
    ghost.setMode(GhostMode::Chase);
    ghost.updateTarget(tileCenterOf(6, 1), Direction::Down, tileCenterOf(1, 1));
    CHECK(nearlyEquals(ghost.target(), tileCenterOf(6, 1)));
}

PAC_TEST(Ghost, pinkyTargetsFourTilesAhead) {
    Maze maze = layout();
    Random random(7);
    Ghost ghost(maze, GhostId::Pinky, 4, 3, Direction::Left, &random);
    ghost.setMode(GhostMode::Chase);

    ghost.updateTarget(tileCenterOf(6, 1), Direction::Left, tileCenterOf(1, 1));
    CHECK(nearlyEquals(ghost.target(), tileCenterOf(2, 1)));

    ghost.updateTarget(tileCenterOf(6, 1), Direction::None, tileCenterOf(1, 1));
    CHECK(nearlyEquals(ghost.target(), tileCenterOf(6, 1)));
}

PAC_TEST(Ghost, inkyTargetsReflectedTile) {
    Maze maze = layout();
    Random random(7);
    Ghost ghost(maze, GhostId::Inky, 4, 3, Direction::Left, &random);
    ghost.setMode(GhostMode::Chase);

    ghost.updateTarget(tileCenterOf(6, 1), Direction::Up, tileCenterOf(4, 3));
    CHECK(nearlyEquals(ghost.target(), tileCenterOf(8, -5)));
}

PAC_TEST(Ghost, clydeSwitchesTargetByDistance) {
    Maze maze = layout();
    Random random(7);
    Ghost ghost(maze, GhostId::Clyde, 4, 3, Direction::Left, &random);
    ghost.setMode(GhostMode::Chase);

    ghost.updateTarget(tileCenterOf(6, 1), Direction::None, tileCenterOf(4, 3));
    CHECK(nearlyEquals(ghost.target(), tileCenterOf(0, 6)));

    ghost.setPosition(tileCenterOf(9, 3));
    ghost.updateTarget(tileCenterOf(1, 1), Direction::None, tileCenterOf(9, 3));
    CHECK(nearlyEquals(ghost.target(), tileCenterOf(1, 1)));
}

PAC_TEST(Ghost, prefersUpWhenTied) {
    Maze maze = junction();
    Random random(11);
    Ghost ghost(maze, GhostId::Blinky, 2, 2, Direction::Up, &random);
    ghost.setSpeed(1.0f);
    ghost.setMode(GhostMode::Chase);
    ghost.updateTarget(tileCenterOf(1, 1), Direction::None, tileCenterOf(1, 1));
    ghost.update(1.0f);

    CHECK_EQ(ghost.tileX(), 2);
    CHECK_EQ(ghost.tileY(), 1);
    CHECK_EQ(ghost.direction(), Direction::Up);
}

PAC_TEST(Ghost, movesTowardsTarget) {
    Maze maze = layout();
    Random random(5);
    Ghost ghost(maze, GhostId::Blinky, 3, 1, Direction::Left, &random);
    ghost.setSpeed(1.0f);
    ghost.setMode(GhostMode::Chase);
    ghost.updateTarget(tileCenterOf(1, 1), Direction::None, tileCenterOf(1, 1));
    ghost.update(1.0f);

    CHECK_EQ(ghost.tileX(), 2);
    CHECK_EQ(ghost.direction(), Direction::Left);
}

PAC_TEST(Ghost, reversesWhenFrightened) {
    Maze maze = layout();
    Random random(5);
    Ghost ghost(maze, GhostId::Blinky, 2, 1, Direction::Right, &random);
    ghost.setSpeed(1.0f);

    ghost.setMode(GhostMode::Frightened);
    CHECK(ghost.isEdible());
    CHECK_EQ(ghost.direction(), Direction::Left);
}

PAC_TEST(Ghost, frightenedChoiceIsDeterministic) {
    Maze maze = junction();
    Random first(2024);
    Random second(2024);

    Ghost a(maze, GhostId::Blinky, 2, 2, Direction::Up, &first);
    Ghost b(maze, GhostId::Blinky, 2, 2, Direction::Up, &second);
    a.setSpeed(1.0f);
    b.setSpeed(1.0f);
    a.setMode(GhostMode::Frightened);
    b.setMode(GhostMode::Frightened);

    for (int i = 0; i < 6; ++i) {
        a.updateTarget(a.position(), Direction::None, a.position());
        b.updateTarget(b.position(), Direction::None, b.position());
        a.update(1.0f);
        b.update(1.0f);
        CHECK_EQ(a.tileX(), b.tileX());
        CHECK_EQ(a.tileY(), b.tileY());
    }
}

PAC_TEST(Ghost, eyesPathHomeThroughDoor) {
    Maze maze = house();
    Random random(31);
    Ghost ghost(maze, GhostId::Pinky, 3, 4, Direction::Up, &random);
    ghost.setSpeed(1.0f);
    ghost.setPosition(tileCenterOf(5, 1));
    ghost.setDirection(Direction::Left);
    ghost.eat();

    CHECK(ghost.isEyes());
    CHECK(!ghost.isEdible());

    for (int i = 0; i < 400 && !ghost.hasReachedSpawn(); ++i) {
        ghost.updateTarget(ghost.position(), Direction::None, ghost.position());
        ghost.update(1.0f);
    }
    CHECK(ghost.hasReachedSpawn());
    CHECK_EQ(ghost.tileX(), 3);
    CHECK_EQ(ghost.tileY(), 4);
}

PAC_TEST(Ghost, resetsAfterDeath) {
    Maze maze = layout();
    Random random(3);
    Ghost ghost(maze, GhostId::Inky, 7, 3, Direction::Left, &random);
    ghost.setSpeed(1.0f);
    ghost.setMode(GhostMode::Frightened);
    ghost.eat();
    ghost.setPosition(tileCenterOf(1, 1));

    ghost.reset();
    CHECK_EQ(ghost.tileX(), 7);
    CHECK_EQ(ghost.tileY(), 3);
    CHECK_EQ(ghost.mode(), GhostMode::Scatter);
    CHECK(!ghost.isEyes());
    CHECK_EQ(ghost.direction(), Direction::Left);
    CHECK(ghost.hasReachedSpawn());
}

PAC_TEST(Ghost, names) {
    CHECK(std::string(toString(GhostId::Blinky)) == "blinky");
    CHECK(std::string(toString(GhostId::Pinky)) == "pinky");
    CHECK(std::string(toString(GhostId::Inky)) == "inky");
    CHECK(std::string(toString(GhostId::Clyde)) == "clyde");
    CHECK(std::string(toString(GhostMode::Scatter)) == "scatter");
    CHECK(std::string(toString(GhostMode::Chase)) == "chase");
    CHECK(std::string(toString(GhostMode::Frightened)) == "frightened");
    CHECK(std::string(toString(GhostMode::Eyes)) == "eyes");
    CHECK(std::string(toString(GhostMode::LeavingHouse)) == "leavinghouse");
}

PAC_TEST(Ghost, eyesTargetTheirSpawn) {
    Maze maze = house();
    Random random(3);
    Ghost ghost(maze, GhostId::Pinky, 3, 4, Direction::Left, &random);

    // The eyes go home.
    ghost.setMode(GhostMode::Eyes);
    ghost.updateTarget(tileCenterOf(1, 1), Direction::None, tileCenterOf(1, 1));
    CHECK(nearlyEquals(ghost.target(), tileCenterOf(3, 4)));

    // A ghost that is leaving the house goes to the tile above it.
    ghost.setMode(GhostMode::LeavingHouse);
    ghost.updateTarget(tileCenterOf(1, 1), Direction::None, tileCenterOf(1, 1));
    CHECK(nearlyEquals(ghost.target(), tileCenterOf(3, 2)));
}

// --- The corners of the rules ---------------------------------------------

PAC_TEST(Ghost, flashesOnlyWhileTheFrightIsAboutToEnd) {
    Maze maze = layout();
    Random random(17);
    Ghost ghost(maze, GhostId::Blinky, 4, 3, Direction::Left, &random);

    // A ghost that is not frightened never flashes.
    CHECK(!ghost.isFrightenedFlashing());

    ghost.setMode(GhostMode::Frightened);

    // The last two seconds of the fright: the ghost flashes.
    ghost.setFrightenedTimeLeft(2.0f);
    CHECK(ghost.isFrightenedFlashing());
    ghost.setFrightenedTimeLeft(1.0f);
    CHECK(ghost.isFrightenedFlashing());

    // More than two seconds left: no flash yet.
    ghost.setFrightenedTimeLeft(2.5f);
    CHECK(!ghost.isFrightenedFlashing());

    // No time left: the fright is over.
    ghost.setFrightenedTimeLeft(0.0f);
    CHECK(!ghost.isFrightenedFlashing());
}

PAC_TEST(Ghost, picksTheFirstChoiceWithoutRandomness) {
    Maze maze = junction();
    Ghost first(maze, GhostId::Blinky, 2, 2, Direction::Up, nullptr);
    Ghost second(maze, GhostId::Blinky, 2, 2, Direction::Up, nullptr);
    first.setSpeed(1.0f);
    second.setSpeed(1.0f);
    first.setMode(GhostMode::Frightened);
    second.setMode(GhostMode::Frightened);

    first.update(1.0f);
    second.update(1.0f);

    // Without a random source the ghost always takes the first way it can
    // go, so both ghosts end up in the same place.
    CHECK_EQ(first.tileX(), 2);
    CHECK_EQ(first.tileY(), 1);
    CHECK_EQ(second.tileX(), first.tileX());
    CHECK_EQ(second.tileY(), first.tileY());
}