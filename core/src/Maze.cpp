#include "pacman/Maze.hpp"

#include <algorithm>
#include <utility>

namespace pac {

Maze::Maze(Level level) : level_(std::move(level)) {
    cols_ = level_.cols();
    rows_ = level_.rows();
    const std::size_t count = static_cast<std::size_t>(cols_) * static_cast<std::size_t>(rows_);
    base_.assign(count, Tile::Wall);
    for (int y = 0; y < rows_; ++y) {
        for (int x = 0; x < cols_; ++x) {
            base_[static_cast<std::size_t>(y) * static_cast<std::size_t>(cols_) + static_cast<std::size_t>(x)] =
                level_.tileAt(x, y);
        }
    }
    resetPellets();
}

void Maze::resetPellets() {
    tiles_ = base_;
    pelletsRemaining_ = 0;
    fruitsRemaining_ = 0;
    fruitsTotal_ = 0;
    for (const Tile tile : tiles_) {
        if (hasPellet(tile)) {
            ++pelletsRemaining_;
        }
        if (isFruit(tile)) {
            ++fruitsRemaining_;
            ++fruitsTotal_;
        }
    }
    pelletsTotal_ = pelletsRemaining_;
}

Tile Maze::rawTile(int x, int y) const {
    if (x < 0 || x >= cols_ || y < 0 || y >= rows_) {
        return Tile::Wall;
    }
    return base_[static_cast<std::size_t>(y) * static_cast<std::size_t>(cols_) + static_cast<std::size_t>(x)];
}

int Maze::wrapX(int x, int y) const {
    if (cols_ <= 0 || rows_ <= 0) {
        return x;
    }
    if (x >= 0 && x < cols_) {
        return x;
    }
    if (y < 0 || y >= rows_) {
        return x;
    }
    const int edge = (x < 0) ? 0 : cols_ - 1;
    if (rawTile(edge, y) != Tile::Tunnel) {
        return x;
    }
    return (x < 0) ? x + cols_ : x - cols_;
}

Tile Maze::tileAt(int x, int y) const {
    if (y < 0 || y >= rows_) {
        return Tile::Wall;
    }
    const int wrapped = wrapX(x, y);
    if (wrapped < 0 || wrapped >= cols_) {
        return Tile::Wall;
    }
    return tiles_[static_cast<std::size_t>(y) * static_cast<std::size_t>(cols_) + static_cast<std::size_t>(wrapped)];
}

Tile Maze::tileAt(Vec2 position) const {
    return tileAt(tileXOf(position), tileYOf(position));
}

bool Maze::canEnter(int x, int y, bool passDoors) const {
    const Tile tile = tileAt(x, y);
    if (isWall(tile)) {
        return false;
    }
    if (isDoor(tile) && !passDoors) {
        return false;
    }
    return true;
}

bool Maze::isTunnel(int x, int y) const {
    return pac::isTunnel(tileAt(x, y));
}

bool Maze::isPellet(int x, int y) const {
    return hasPellet(tileAt(x, y));
}

int Maze::tileXOf(Vec2 position) const {
    return wrapX(tileIndexOf(position.x), tileIndexOf(position.y));
}

int Maze::tileYOf(Vec2 position) const {
    return tileIndexOf(position.y);
}

Vec2 Maze::wrapPosition(Vec2 position) const {
    if (cols_ <= 0) {
        return position;
    }
    if (position.x < 0.0f) {
        position.x += static_cast<float>(cols_);
    } else if (position.x >= static_cast<float>(cols_)) {
        position.x -= static_cast<float>(cols_);
    }
    return position;
}

Tile Maze::takePellet(int x, int y) {
    if (y < 0 || y >= rows_) {
        return Tile::Empty;
    }
    const int wrapped = wrapX(x, y);
    if (wrapped < 0 || wrapped >= cols_) {
        return Tile::Empty;
    }
    Tile& tile = tiles_[static_cast<std::size_t>(y) * static_cast<std::size_t>(cols_) + static_cast<std::size_t>(wrapped)];
    if (hasPellet(tile)) {
        const Tile taken = tile;
        tile = Tile::Empty;
        --pelletsRemaining_;
        return taken;
    }
    if (isFruit(tile)) {
        const Tile taken = tile;
        tile = Tile::Empty;
        --fruitsRemaining_;
        return taken;
    }
    return Tile::Empty;
}

Direction Maze::stepToward(int fromX, int fromY, int toX, int toY, bool passDoors) const {
    if (cols_ <= 0 || rows_ <= 0) {
        return Direction::None;
    }

    const int startX = wrapX(fromX, fromY);
    const int goalX = wrapX(toX, toY);
    if (startX < 0 || fromY < 0 || fromY >= rows_ || goalX < 0 || toY < 0 || toY >= rows_) {
        return Direction::None;
    }
    if (startX == goalX && fromY == toY) {
        return Direction::None;
    }

    const std::size_t count = static_cast<std::size_t>(cols_) * static_cast<std::size_t>(rows_);
    std::vector<int> parent(count, -1);
    std::vector<Direction> parentDirection(count, Direction::None);
    std::vector<bool> visited(count, false);
    std::vector<int> queue;
    queue.reserve(count);

    const int start = fromY * cols_ + startX;
    visited[static_cast<std::size_t>(start)] = true;
    queue.push_back(start);

    int goal = -1;
    for (std::size_t head = 0; head < queue.size() && goal < 0; ++head) {
        const int current = queue[head];
        const int cx = current % cols_;
        const int cy = current / cols_;
        for (const Direction direction : kDirections) {
            const int ny = cy + dirY(direction);
            if (ny < 0 || ny >= rows_) {
                continue;
            }
            const int nx = wrapX(cx + dirX(direction), ny);
            if (nx < 0 || nx >= cols_) {
                continue;
            }
            if (!canEnter(nx, ny, passDoors)) {
                continue;
            }
            const int next = ny * cols_ + nx;
            if (visited[static_cast<std::size_t>(next)]) {
                continue;
            }
            visited[static_cast<std::size_t>(next)] = true;
            parent[static_cast<std::size_t>(next)] = current;
            parentDirection[static_cast<std::size_t>(next)] = direction;
            if (nx == goalX && ny == toY) {
                goal = next;
                break;
            }
            queue.push_back(next);
        }
    }

    if (goal < 0) {
        return Direction::None;
    }

    int cursor = goal;
    while (cursor != start) {
        const int previous = parent[static_cast<std::size_t>(cursor)];
        if (previous < 0) {
            return Direction::None;
        }
        if (previous == start) {
            return parentDirection[static_cast<std::size_t>(cursor)];
        }
        cursor = previous;
    }
    return Direction::None;
}

}