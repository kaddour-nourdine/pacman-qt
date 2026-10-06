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
    CHECK_EQ(toString(GhostId::Blinky)[0], 'b');
    CHECK_EQ(toString(GhostMode::Eyes)[0], 'e');
    CHECK_EQ(toString(GhostMode::Chase)[0], 'c');
}