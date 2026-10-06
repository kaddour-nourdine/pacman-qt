#include "Fixtures.hpp"
#include "TestFramework.hpp"
#include "pacman/Player.hpp"

using namespace pac;

namespace {

Maze layout() {
    return Maze(pactest::makeLevel(pactest::layoutMap(), __FILE__, __LINE__));
}

Maze ring() {
    return Maze(pactest::makeLevel(pactest::ringMap(), __FILE__, __LINE__));
}

}  // namespace

PAC_TEST(Player, spawnsFromLevel) {
    Maze maze = layout();
    Player player(maze);
    CHECK_EQ(player.tileX(), 6);
    CHECK_EQ(player.tileY(), 1);
    CHECK_EQ(player.direction(), Direction::Left);
    CHECK(player.isAtTileCenter());
    CHECK_NEAR(player.speed(), 6.0, 1e-6);
    CHECK_EQ(player.pelletsEaten(), 0);
}

PAC_TEST(Player, movesPartialAndWholeTiles) {
    Maze maze = layout();
    Player player(maze);
    player.setSpeed(1.0f);

    player.update(0.5f);
    CHECK_NEAR(player.position().x, 6.0, 1e-4);
    CHECK_NEAR(player.position().y, 1.5, 1e-4);
    CHECK(!player.isAtTileCenter());

    player.update(0.5f);
    CHECK_NEAR(player.position().x, 5.5, 1e-4);
    CHECK(player.isAtTileCenter());
    CHECK_EQ(player.tileX(), 5);
}

PAC_TEST(Player, movesExactlyOneTilePerSecond) {
    Maze maze = layout();
    Player player(maze);
    player.setSpeed(1.0f);

    for (int i = 0; i < 3; ++i) {
        player.update(1.0f);
        CHECK(player.isAtTileCenter());
    }
    CHECK_EQ(player.tileX(), 3);
}

PAC_TEST(Player, appliesRequestedTurnAtNextCenter) {
    Maze maze = layout();
    Player player(maze);
    player.setSpeed(1.0f);

    player.update(0.5f);
    player.requestDirection(Direction::Right);
    player.update(0.5f);

    CHECK_EQ(player.tileX(), 5);
    CHECK_EQ(player.direction(), Direction::Left);

    player.update(1.0f);
    CHECK_EQ(player.tileX(), 6);
    CHECK_EQ(player.direction(), Direction::Right);
}

PAC_TEST(Player, ignoresTurnIntoWall) {
    Maze maze = layout();
    Player player(maze);
    player.setSpeed(1.0f);

    player.requestDirection(Direction::Down);
    player.update(1.0f);

    CHECK_EQ(player.tileX(), 5);
    CHECK_EQ(player.tileY(), 1);
    CHECK_EQ(player.direction(), Direction::Left);
}

PAC_TEST(Player, stopsAtWall) {
    Maze maze = layout();
    Player player(maze);
    player.setSpeed(1.0f);

    for (int i = 0; i < 5; ++i) {
        player.update(1.0f);
    }
    CHECK_EQ(player.tileX(), 1);
    CHECK_EQ(player.direction(), Direction::Left);

    player.update(1.0f);
    CHECK_EQ(player.tileX(), 1);
    CHECK(player.isAtTileCenter());
    CHECK(player.isBlocked());
    CHECK_EQ(player.direction(), Direction::None);
}

PAC_TEST(Player, restartsWhenBlocked) {
    Maze maze = layout();
    Player player(maze);
    player.setSpeed(1.0f);

    for (int i = 0; i < 6; ++i) {
        player.update(1.0f);
    }
    CHECK(player.isBlocked());

    player.requestDirection(Direction::Right);
    player.update(1.0f);
    CHECK_EQ(player.tileX(), 2);
    CHECK_EQ(player.direction(), Direction::Right);
}

PAC_TEST(Player, wrapsThroughTunnel) {
    Maze maze = ring();
    Player player(maze);
    player.setSpeed(1.0f);

    CHECK_EQ(player.tileX(), 4);
    for (int i = 0; i < 11; ++i) {
        player.update(1.0f);
        CHECK(player.isAtTileCenter());
    }
    CHECK_EQ(player.tileX(), 4);
    CHECK_EQ(player.tileY(), 1);
}

PAC_TEST(Player, cannotEnterGhostDoor) {
    Maze maze = pac::Maze(pactest::makeLevel(pactest::houseMap(), __FILE__, __LINE__));
    Player player(maze);
    player.setSpeed(1.0f);

    CHECK_EQ(player.tileX(), 5);
    player.requestDirection(Direction::Down);
    player.update(1.0f);
    CHECK_EQ(player.tileY(), 2);

    player.update(1.0f);
    CHECK_EQ(player.tileY(), 2);
    CHECK_EQ(player.tileX(), 5);
    CHECK(player.isBlocked());
    CHECK_EQ(player.direction(), Direction::None);
}

PAC_TEST(Player, tracksPellets) {
    Maze maze = layout();
    Player player(maze);

    player.notePelletEaten(Tile::Pellet);
    player.notePelletEaten(Tile::PowerPellet);
    player.notePelletEaten(Tile::Empty);
    CHECK_EQ(player.pelletsEaten(), 2);
    CHECK_EQ(player.powerPelletsEaten(), 1);
}

PAC_TEST(Player, animationPhaseStaysNormalized) {
    Maze maze = layout();
    Player player(maze);
    player.setSpeed(1.0f);

    CHECK_NEAR(player.animationPhase(), 0.0, 1e-6);
    for (int i = 0; i < 5; ++i) {
        player.update(1.0f);
        const float phase = player.animationPhase();
        CHECK(phase >= 0.0f);
        CHECK(phase < 1.0f);
    }
}

PAC_TEST(Player, resetsToStart) {
    Maze maze = layout();
    Player player(maze);
    player.setSpeed(1.0f);

    player.update(2.0f);
    player.requestDirection(Direction::Down);
    player.update(1.0f);
    player.reset();

    CHECK_EQ(player.tileX(), 6);
    CHECK_EQ(player.tileY(), 1);
    CHECK_EQ(player.direction(), Direction::Left);
    CHECK_EQ(player.requestedDirection(), Direction::None);
    CHECK_NEAR(player.distanceTravelled(), 0.0, 1e-6);
    CHECK_NEAR(player.speed(), 6.0, 1e-6);
}