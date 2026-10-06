#include "pacman/Ghost.hpp"

namespace pac {

namespace {

constexpr int kPointValues[4] = {200, 400, 800, 1600};

}  // namespace

const char* toString(GhostId id) {
    switch (id) {
    case GhostId::Blinky:
        return "blinky";
    case GhostId::Pinky:
        return "pinky";
    case GhostId::Inky:
        return "inky";
    case GhostId::Clyde:
        break;
    }
    return "clyde";
}

const char* toString(GhostMode mode) {
    switch (mode) {
    case GhostMode::Scatter:
        return "scatter";
    case GhostMode::Chase:
        return "chase";
    case GhostMode::Frightened:
        return "frightened";
    case GhostMode::Eyes:
        break;
    }
    return "eyes";
}

Ghost::Ghost(Maze& maze, GhostId id, int spawnX, int spawnY, Direction spawnDirection, Random* random)
    : Entity(maze, tileCenterOf(spawnX, spawnY), spawnDirection, 0.0f),
      id_(id),
      spawnX_(spawnX),
      spawnY_(spawnY),
      spawnDirection_(spawnDirection),
      random_(random) {
    reset();
}

void Ghost::reset() {
    setPosition(tileCenterOf(spawnX_, spawnY_));
    setDirection(spawnDirection_);
    clearRequestedDirection();
    distanceTravelled_ = 0.0f;
    mode_ = GhostMode::Scatter;
    playerPosition_ = position();
    blinkyPosition_ = position();
    target_ = scatterCorner();
}

void Ghost::setMode(GhostMode mode) {
    if (mode_ == mode) {
        return;
    }
    mode_ = mode;
    if (mode == GhostMode::Frightened || mode == GhostMode::Eyes) {
        reverse();
    }
}

Vec2 Ghost::scatterCorner() const {
    const int right = (maze_ != nullptr ? maze_->cols() : 1) - 1;
    const int bottom = (maze_ != nullptr ? maze_->rows() : 1) - 1;
    switch (id_) {
    case GhostId::Blinky:
        return tileCenterOf(right, 0);
    case GhostId::Pinky:
        return tileCenterOf(0, 0);
    case GhostId::Inky:
        return tileCenterOf(right, bottom);
    case GhostId::Clyde:
        break;
    }
    return tileCenterOf(0, bottom);
}

Vec2 Ghost::chaseTarget(Vec2 playerPosition, Direction playerDirection, Vec2 blinkyPosition) const {
    const int px = tileIndexOf(playerPosition.x);
    const int py = tileIndexOf(playerPosition.y);

    switch (id_) {
    case GhostId::Blinky:
        return tileCenterOf(px, py);
    case GhostId::Pinky: {
        const int tx = px + 4 * dirX(playerDirection);
        const int ty = py + 4 * dirY(playerDirection);
        return tileCenterOf(tx, ty);
    }
    case GhostId::Inky: {
        const int ax = px + 2 * dirX(playerDirection);
        const int ay = py + 2 * dirY(playerDirection);
        const int bx = tileIndexOf(blinkyPosition.x);
        const int by = tileIndexOf(blinkyPosition.y);
        return tileCenterOf(2 * ax - bx, 2 * ay - by);
    }
    case GhostId::Clyde: {
        const int gx = tileX();
        const int gy = tileY();
        const float dx = static_cast<float>(gx - px);
        const float dy = static_cast<float>(gy - py);
        if (dx * dx + dy * dy > 64.0f) {
            return tileCenterOf(px, py);
        }
        return scatterCorner();
    }
    }
    return tileCenterOf(px, py);
}

void Ghost::updateTarget(Vec2 playerPosition, Direction playerDirection, Vec2 blinkyPosition) {
    playerPosition_ = playerPosition;
    blinkyPosition_ = blinkyPosition;

    switch (mode_) {
    case GhostMode::Scatter:
        target_ = scatterCorner();
        break;
    case GhostMode::Chase:
        target_ = chaseTarget(playerPosition, playerDirection, blinkyPosition);
        break;
    case GhostMode::Frightened:
        target_ = position();
        break;
    case GhostMode::Eyes:
        target_ = tileCenterOf(spawnX_, spawnY_);
        break;
    }
}

int Ghost::pointsValue() const {
    return kPointValues[static_cast<int>(id_)];
}

void Ghost::eat() {
    clearRequestedDirection();
    setMode(GhostMode::Eyes);
}

bool Ghost::hasReachedSpawn() const {
    return tileX() == spawnX_ && tileY() == spawnY_;
}

void Ghost::onArriveAtCenter() {
    if (mode_ == GhostMode::Frightened) {
        reverse();
    }
}

Direction Ghost::frightenedDirection() const {
    const Direction forbidden = opposite(direction());
    Direction options[3];
    int count = 0;
    for (const Direction candidate : kGhostPreference) {
        if (candidate == forbidden) {
            continue;
        }
        if (!canMove(candidate)) {
            continue;
        }
        options[count] = candidate;
        ++count;
    }
    if (count == 0) {
        return canMove(forbidden) ? forbidden : Direction::None;
    }
    if (random_ == nullptr) {
        return options[0];
    }
    return options[random_->range(0, count - 1)];
}

Direction Ghost::chooseDirection() const {
    if (maze_ == nullptr) {
        return Direction::None;
    }

    if (mode_ == GhostMode::Eyes) {
        return maze_->stepToward(tileX(), tileY(), spawnX_, spawnY_, true);
    }

    if (mode_ == GhostMode::Frightened) {
        return frightenedDirection();
    }

    const Direction forbidden = opposite(direction());
    Direction best = Direction::None;
    float bestDistance = 0.0f;
    for (const Direction candidate : kGhostPreference) {
        if (candidate == forbidden || !canMove(candidate)) {
            continue;
        }
        const float nx = static_cast<float>(tileX() + dirX(candidate)) + 0.5f;
        const float ny = static_cast<float>(tileY() + dirY(candidate)) + 0.5f;
        const float dx = nx - target_.x;
        const float dy = ny - target_.y;
        const float distanceSquared = dx * dx + dy * dy;
        if (best == Direction::None || distanceSquared < bestDistance) {
            best = candidate;
            bestDistance = distanceSquared;
        }
    }

    if (best == Direction::None && canMove(forbidden)) {
        return forbidden;
    }
    return best;
}

}