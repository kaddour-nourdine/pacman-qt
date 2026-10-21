#include "pacman/Entity.hpp"

#include <algorithm>
#include <cmath>

namespace pac {

namespace {

constexpr float kEpsilon = 1e-4f;

} // namespace

Entity::Entity(Maze& maze, Vec2 position, Direction direction, float speed)
    : maze_(&maze), position_(maze.wrapPosition(position)), direction_(direction), speed_(speed) {}

void Entity::setPosition(Vec2 position) {
    if (maze_ != nullptr) {
        position = maze_->wrapPosition(position);
    }
    position_ = position;
}

int Entity::tileX() const {
    if (maze_ == nullptr) {
        return tileIndexOf(position_.x);
    }
    return maze_->tileXOf(position_);
}

int Entity::tileY() const {
    if (maze_ == nullptr) {
        return tileIndexOf(position_.y);
    }
    return maze_->tileYOf(position_);
}

bool Entity::isAtTileCenter() const {
    const int tx = tileX();
    const int ty = tileY();
    const float cx = static_cast<float>(tx) + 0.5f;
    const float cy = static_cast<float>(ty) + 0.5f;
    return std::fabs(position_.x - cx) <= kEpsilon && std::fabs(position_.y - cy) <= kEpsilon;
}

bool Entity::canMove(Direction direction) const {
    if (maze_ == nullptr || direction == Direction::None) {
        return false;
    }
    return maze_->canEnter(tileX() + dirX(direction), tileY() + dirY(direction), canPassDoors());
}

void Entity::reverse() {
    if (maze_ == nullptr || direction_ == Direction::None) {
        return;
    }
    const Direction reversed = opposite(direction_);
    if (canMove(reversed)) {
        direction_ = reversed;
        requestedDirection_ = Direction::None;
    }
}

Direction Entity::chooseDirection() const {
    if (requestedDirection_ != Direction::None && canMove(requestedDirection_)) {
        return requestedDirection_;
    }
    if (direction_ != Direction::None && canMove(direction_)) {
        return direction_;
    }
    return Direction::None;
}

void Entity::advance(float distance) {
    if (distance <= 0.0f) {
        return;
    }
    position_.x += static_cast<float>(dirX(direction_)) * distance;
    position_.y += static_cast<float>(dirY(direction_)) * distance;
    if (maze_ != nullptr) {
        position_ = maze_->wrapPosition(position_);
    }
    distanceTravelled_ += distance;
}

float Entity::distanceToNextCenter() const {
    const int tx = tileX();
    const int ty = tileY();
    switch (direction_) {
    case Direction::Right: {
        const float center = static_cast<float>(tx) + 0.5f;
        const float next = position_.x < center ? center : center + 1.0f;
        return next - position_.x;
    }
    case Direction::Left: {
        const float center = static_cast<float>(tx) + 0.5f;
        const float next = position_.x > center ? center : center - 1.0f;
        return position_.x - next;
    }
    case Direction::Down: {
        const float center = static_cast<float>(ty) + 0.5f;
        const float next = position_.y < center ? center : center + 1.0f;
        return next - position_.y;
    }
    case Direction::Up: {
        const float center = static_cast<float>(ty) + 0.5f;
        const float next = position_.y > center ? center : center - 1.0f;
        return position_.y - next;
    }
    case Direction::None:
        break;
    }
    return 0.0f;
}

void Entity::snapToNextCenter() {
    switch (direction_) {
    case Direction::Right:
    case Direction::Left:
        position_.x = static_cast<float>(tileX()) + 0.5f;
        break;
    case Direction::Down:
    case Direction::Up:
        position_.y = static_cast<float>(tileY()) + 0.5f;
        break;
    case Direction::None:
        break;
    }
}

void Entity::update(float deltaSeconds) {
    if (maze_ == nullptr || deltaSeconds <= 0.0f || speed_ <= 0.0f) {
        return;
    }

    float remaining = deltaSeconds * speed_;
    for (int guard = 0; remaining > kEpsilon && guard < 512; ++guard) {
        if (!isAtTileCenter()) {
            if (direction_ == Direction::None) {
                return;
            }
            const float need = distanceToNextCenter();
            const float step = std::min(remaining, need);
            advance(step);
            remaining -= step;
            if (isAtTileCenter()) {
                snapToNextCenter();
            }
            continue;
        }

        onArriveAtCenter();
        const Direction chosen = chooseDirection();
        if (chosen == Direction::None) {
            direction_ = Direction::None;
            return;
        }
        direction_ = chosen;

        const float need = distanceToNextCenter();
        if (remaining < need) {
            advance(remaining);
            remaining = 0.0f;
        } else {
            advance(need);
            remaining -= need;
            snapToNextCenter();
        }
    }
}

} // namespace pac