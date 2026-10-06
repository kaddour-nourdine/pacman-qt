#include <string>

#include "Fixtures.hpp"
#include "TestFramework.hpp"
#include "pacman/Direction.hpp"
#include "pacman/Tile.hpp"
#include "pacman/Vec2.hpp"

using namespace pac;

PAC_TEST(Direction, opposites) {
    CHECK_EQ(opposite(Direction::Up), Direction::Down);
    CHECK_EQ(opposite(Direction::Down), Direction::Up);
    CHECK_EQ(opposite(Direction::Left), Direction::Right);
    CHECK_EQ(opposite(Direction::Right), Direction::Left);
    CHECK_EQ(opposite(Direction::None), Direction::None);
    CHECK_EQ(opposite(opposite(Direction::Up)), Direction::Up);
}

PAC_TEST(Direction, deltas) {
    CHECK_EQ(dirX(Direction::Left), -1);
    CHECK_EQ(dirX(Direction::Right), 1);
    CHECK_EQ(dirX(Direction::Up), 0);
    CHECK_EQ(dirY(Direction::Up), -1);
    CHECK_EQ(dirY(Direction::Down), 1);
    CHECK_EQ(dirY(Direction::Left), 0);
}

PAC_TEST(Direction, axes) {
    CHECK(isHorizontal(Direction::Left));
    CHECK(isHorizontal(Direction::Right));
    CHECK(!isHorizontal(Direction::Up));
    CHECK(isVertical(Direction::Up));
    CHECK(isVertical(Direction::Down));
    CHECK(!isVertical(Direction::Left));
}

PAC_TEST(Direction, fromDelta) {
    CHECK_EQ(directionFromDelta(-1, 0), Direction::Left);
    CHECK_EQ(directionFromDelta(1, 0), Direction::Right);
    CHECK_EQ(directionFromDelta(0, -1), Direction::Up);
    CHECK_EQ(directionFromDelta(0, 1), Direction::Down);
    CHECK_EQ(directionFromDelta(0, 0), Direction::None);
}

PAC_TEST(Vec2, arithmetic) {
    const Vec2 a{1.0f, 2.0f};
    const Vec2 b{0.5f, -1.0f};
    CHECK(nearlyEquals(a + b, Vec2{1.5f, 1.0f}));
    CHECK(nearlyEquals(a - b, Vec2{0.5f, 3.0f}));
    CHECK(nearlyEquals(a * 2.0f, Vec2{2.0f, 4.0f}));
    CHECK_NEAR(length(Vec2{3.0f, 4.0f}), 5.0, 1e-6);
    CHECK_NEAR(distance(Vec2{0.0f, 0.0f}, Vec2{3.0f, 4.0f}), 5.0, 1e-6);
}

PAC_TEST(Vec2, tileHelpers) {
    CHECK(nearlyEquals(tileCenterOf(2, 3), Vec2{2.5f, 3.5f}));
    CHECK_EQ(tileIndexOf(2.5f), 2);
    CHECK_EQ(tileIndexOf(0.5f), 0);
    CHECK_EQ(tileIndexOf(-0.5f), -1);
}

PAC_TEST(Tile, classification) {
    CHECK(isWall(Tile::Wall));
    CHECK(!isWall(Tile::Door));
    CHECK(isWalkable(Tile::Empty));
    CHECK(isWalkable(Tile::Door));
    CHECK(blocksGhost(Tile::Door));
    CHECK(blocksGhost(Tile::Wall));
    CHECK(!blocksGhost(Tile::Empty));
    CHECK(hasPellet(Tile::Pellet));
    CHECK(hasPellet(Tile::PowerPellet));
    CHECK(!hasPellet(Tile::Empty));
    CHECK(isPowerPellet(Tile::PowerPellet));
    CHECK(!isPowerPellet(Tile::Pellet));
    CHECK(isTunnel(Tile::Tunnel));
}

PAC_TEST(Tile, characterRoundTrip) {
    CHECK_EQ(charToTile('#'), Tile::Wall);
    CHECK_EQ(charToTile('.'), Tile::Pellet);
    CHECK_EQ(charToTile('o'), Tile::PowerPellet);
    CHECK_EQ(charToTile('-'), Tile::Door);
    CHECK_EQ(charToTile('_'), Tile::Tunnel);
    CHECK_EQ(charToTile(' '), Tile::Empty);
    CHECK_EQ(tileToChar(Tile::Wall), '#');
    CHECK_EQ(tileToChar(Tile::PowerPellet), 'o');
    CHECK(isMapChar('1'));
    CHECK(isMapChar('P'));
    CHECK(!isMapChar('='));
    CHECK(!isMapChar('x'));
}