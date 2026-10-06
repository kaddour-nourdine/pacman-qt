#pragma once

#include "pacman/Entity.hpp"
#include "pacman/Random.hpp"

namespace pac {

enum class GhostId {
    Blinky = 0,
    Pinky = 1,
    Inky = 2,
    Clyde = 3,
};

enum class GhostMode {
    Scatter,
    Chase,
    Frightened,
    Eyes,
    LeavingHouse,
};

const char* toString(GhostId id);
const char* toString(GhostMode mode);

class Ghost : public Entity {
public:
    Ghost(Maze& maze, GhostId id, int spawnX, int spawnY, Direction spawnDirection, Random* random);

    void reset();
    void setMode(GhostMode mode);
    GhostMode mode() const { return mode_; }
    GhostId id() const { return id_; }

    bool isFrightened() const { return mode_ == GhostMode::Frightened; }
    bool isEdible() const { return mode_ == GhostMode::Frightened; }
    bool isEyes() const { return mode_ == GhostMode::Eyes; }
    bool isLeavingHouse() const { return mode_ == GhostMode::LeavingHouse; }
    bool isReleased() const { return released_; }
    void setReleased(bool released) { released_ = released; }

    void updateTarget(Vec2 playerPosition, Direction playerDirection, Vec2 blinkyPosition);

    Vec2 target() const { return target_; }
    Vec2 scatterCorner() const;

    int pointsValue() const;
    void eat();
    bool hasReachedSpawn() const;
    int spawnX() const { return spawnX_; }
    int spawnY() const { return spawnY_; }

protected:
    void onArriveAtCenter() override;
    Direction chooseDirection() const override;
    bool canPassDoors() const override { return true; }

private:
    Vec2 chaseTarget(Vec2 playerPosition, Direction playerDirection, Vec2 blinkyPosition) const;
    Direction frightenedDirection() const;

    GhostId id_ = GhostId::Blinky;
    int spawnX_ = 0;
    int spawnY_ = 0;
    Direction spawnDirection_ = Direction::Left;
    Random* random_ = nullptr;
    GhostMode mode_ = GhostMode::Scatter;
    Vec2 playerPosition_;
    Vec2 blinkyPosition_;
    Vec2 target_;
    bool released_ = true;
};

}