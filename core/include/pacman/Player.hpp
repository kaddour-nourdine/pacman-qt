#pragma once

#include "pacman/Entity.hpp"
#include "pacman/Tile.hpp"

namespace pac {

class Player : public Entity {
public:
    explicit Player(Maze& maze);

    void reset();
    void notePelletEaten(Tile tile);

    int pelletsEaten() const { return pelletsEaten_; }
    int powerPelletsEaten() const { return powerPelletsEaten_; }

    float animationPhase() const;

private:
    int pelletsEaten_ = 0;
    int powerPelletsEaten_ = 0;
};

}