#include "pacman/Player.hpp"

#include <cmath>

namespace pac {

Player::Player(Maze& maze)
    : Entity(maze,
             tileCenterOf(maze.level().playerX(), maze.level().playerY()),
             maze.level().playerDirection(),
             maze.level().config().playerSpeed) {
    reset();
}

void Player::reset() {
    const Level& level = maze_->level();
    setPosition(tileCenterOf(level.playerX(), level.playerY()));
    setDirection(level.playerDirection());
    clearRequestedDirection();
    setSpeed(level.config().playerSpeed);
    distanceTravelled_ = 0.0f;
    pelletsEaten_ = 0;
    powerPelletsEaten_ = 0;
}

void Player::notePelletEaten(Tile tile) {
    if (!hasPellet(tile)) {
        return;
    }
    ++pelletsEaten_;
    if (isPowerPellet(tile)) {
        ++powerPelletsEaten_;
    }
}

float Player::animationPhase() const {
    constexpr float kCyclesPerTile = 0.5f;
    const float phase = std::fmod(distanceTravelled_ * kCyclesPerTile, 1.0f);
    return phase < 0.0f ? phase + 1.0f : phase;
}

}