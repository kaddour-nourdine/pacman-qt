#pragma once

#include "pacman/Direction.hpp"
#include "pacman/Maze.hpp"
#include "pacman/Vec2.hpp"

namespace pac {

class Entity {
public:
    Entity(Maze& maze, Vec2 position, Direction direction, float speed);

    virtual ~Entity() = default;

    Entity(const Entity&) = delete;
    Entity& operator=(const Entity&) = delete;
    Entity(Entity&&) = default;
    Entity& operator=(Entity&&) = default;

    Vec2 position() const { return position_; }
    void setPosition(Vec2 position);

    Direction direction() const { return direction_; }
    void setDirection(Direction direction) { direction_ = direction; }

    Direction requestedDirection() const { return requestedDirection_; }
    void requestDirection(Direction direction) { requestedDirection_ = direction; }
    void clearRequestedDirection() { requestedDirection_ = Direction::None; }

    float speed() const { return speed_; }
    void setSpeed(float speed) { speed_ = speed; }

    int tileX() const;
    int tileY() const;
    Vec2 tileCenter() const { return tileCenterOf(tileX(), tileY()); }
    bool isAtTileCenter() const;
    bool isBlocked() const { return direction_ == Direction::None; }
    float distanceTravelled() const { return distanceTravelled_; }

    bool canMove(Direction direction) const;
    void reverse();

    void update(float deltaSeconds);

protected:
    virtual void onArriveAtCenter() {}
    virtual Direction chooseDirection() const;
    virtual bool canPassDoors() const { return false; }

    void advance(float distance);
    float distanceToNextCenter() const;
    void snapToNextCenter();

    Maze* maze_ = nullptr;
    Vec2 position_;
    Direction direction_ = Direction::None;
    Direction requestedDirection_ = Direction::None;
    float speed_ = 0.0f;
    float distanceTravelled_ = 0.0f;
};

}