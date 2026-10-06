#pragma once

#include <cstdint>

namespace pac {

enum class Tile : std::uint8_t {
    Empty,
    Wall,
    Pellet,
    PowerPellet,
    Door,
    Tunnel,
    Fruit,
};

constexpr bool isWall(Tile tile) {
    return tile == Tile::Wall;
}

constexpr bool isWalkable(Tile tile) {
    return tile != Tile::Wall;
}

constexpr bool blocksGhost(Tile tile) {
    return tile == Tile::Wall || tile == Tile::Door;
}

constexpr bool isDoor(Tile tile) {
    return tile == Tile::Door;
}

constexpr bool isTunnel(Tile tile) {
    return tile == Tile::Tunnel;
}

constexpr bool hasPellet(Tile tile) {
    return tile == Tile::Pellet || tile == Tile::PowerPellet;
}

constexpr bool isFruit(Tile tile) {
    return tile == Tile::Fruit;
}

constexpr bool isPowerPellet(Tile tile) {
    return tile == Tile::PowerPellet;
}

constexpr char tileToChar(Tile tile) {
    switch (tile) {
    case Tile::Wall:
        return '#';
    case Tile::Pellet:
        return '.';
    case Tile::PowerPellet:
        return 'o';
    case Tile::Door:
        return '-';
    case Tile::Tunnel:
        return '_';
    case Tile::Fruit:
        return 'F';
    case Tile::Empty:
        break;
    }
    return ' ';
}

constexpr bool isGhostMarker(char c) {
    return c >= '1' && c <= '4';
}

constexpr bool isMapChar(char c) {
    return c == ' ' || c == '#' || c == '.' || c == 'o' || c == '-' || c == '_' || c == 'P' || c == 'F' || c == 'f' || isGhostMarker(c);
}

constexpr Tile charToTile(char c) {
    switch (c) {
    case '#':
        return Tile::Wall;
    case '.':
        return Tile::Pellet;
    case 'o':
        return Tile::PowerPellet;
    case '-':
        return Tile::Door;
    case '_':
        return Tile::Tunnel;
    case 'F':
    case 'f':
        return Tile::Fruit;
    default:
        break;
    }
    return Tile::Empty;
}

}