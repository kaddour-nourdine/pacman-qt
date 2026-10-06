#pragma once

#include <vector>

#include "pacman/Direction.hpp"
#include "pacman/Level.hpp"
#include "pacman/Tile.hpp"
#include "pacman/Vec2.hpp"

namespace pac {

class Maze {
public:
    explicit Maze(Level level);

    int cols() const { return cols_; }
    int rows() const { return rows_; }

    Tile tileAt(int x, int y) const;
    Tile tileAt(Vec2 position) const;
    Vec2 tileCenter(int x, int y) const { return tileCenterOf(x, y); }

    bool canEnter(int x, int y, bool passDoors) const;
    bool isTunnel(int x, int y) const;
    bool isPellet(int x, int y) const;

    int wrapX(int x, int y) const;
    int tileXOf(Vec2 position) const;
    int tileYOf(Vec2 position) const;
    Vec2 wrapPosition(Vec2 position) const;

    Tile takePellet(int x, int y);

    int pelletsRemaining() const { return pelletsRemaining_; }
    int pelletsTotal() const { return pelletsTotal_; }
    int pelletsEaten() const { return pelletsTotal_ - pelletsRemaining_; }
    bool allPelletsEaten() const { return pelletsTotal_ > 0 && pelletsRemaining_ == 0; }

    void resetPellets();

    Direction stepToward(int fromX, int fromY, int toX, int toY, bool passDoors) const;

    const Level& level() const { return level_; }

private:
    Tile rawTile(int x, int y) const;

    Level level_;
    std::vector<Tile> base_;
    std::vector<Tile> tiles_;
    int cols_ = 0;
    int rows_ = 0;
    int pelletsTotal_ = 0;
    int pelletsRemaining_ = 0;
    int fruitsRemaining_ = 0;
    int fruitsTotal_ = 0;
};

}