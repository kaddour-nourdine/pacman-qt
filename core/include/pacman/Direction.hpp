#pragma once

namespace pac {

enum class Direction {
    None = 0,
    Up,
    Right,
    Down,
    Left,
};

inline constexpr Direction kDirections[] = {Direction::Up, Direction::Right, Direction::Down,
                                            Direction::Left};

inline constexpr Direction kGhostPreference[] = {Direction::Up, Direction::Left, Direction::Down,
                                                 Direction::Right};

constexpr bool isHorizontal(Direction d) {
    return d == Direction::Left || d == Direction::Right;
}

constexpr bool isVertical(Direction d) {
    return d == Direction::Up || d == Direction::Down;
}

constexpr Direction opposite(Direction d) {
    switch (d) {
    case Direction::Up:
        return Direction::Down;
    case Direction::Down:
        return Direction::Up;
    case Direction::Left:
        return Direction::Right;
    case Direction::Right:
        return Direction::Left;
    case Direction::None:
        break;
    }
    return Direction::None;
}

constexpr int dirX(Direction d) {
    switch (d) {
    case Direction::Left:
        return -1;
    case Direction::Right:
        return 1;
    default:
        return 0;
    }
}

constexpr int dirY(Direction d) {
    switch (d) {
    case Direction::Up:
        return -1;
    case Direction::Down:
        return 1;
    default:
        return 0;
    }
}

constexpr Direction directionFromDelta(int dx, int dy) {
    if (dx < 0) {
        return Direction::Left;
    }
    if (dx > 0) {
        return Direction::Right;
    }
    if (dy < 0) {
        return Direction::Up;
    }
    if (dy > 0) {
        return Direction::Down;
    }
    return Direction::None;
}

constexpr const char* toString(Direction d) {
    switch (d) {
    case Direction::Up:
        return "up";
    case Direction::Down:
        return "down";
    case Direction::Left:
        return "left";
    case Direction::Right:
        return "right";
    case Direction::None:
        break;
    }
    return "none";
}

} // namespace pac