#include "TestFramework.hpp"
#include "pacman/Level.hpp"
#include "pacman/Maze.hpp"
#include "pacman/Tile.hpp"
#include <queue>
#include <vector>
#include <filesystem>
#include <string>

using namespace pac;

namespace {
std::vector<std::vector<bool>> reachability(const Level& level, bool passDoors) {
    const int cols = level.cols();
    const int rows = level.rows();
    std::vector<std::vector<bool>> vis(rows, std::vector<bool>(cols, false));
    Maze m(level);
    const int px = level.playerX();
    const int py = level.playerY();
    if (px < 0 || px >= cols || py < 0 || py >= rows) {
        return vis;
    }
    std::queue<std::pair<int, int>> q;
    if (m.canEnter(px, py, passDoors)) {
        vis[py][px] = true;
        q.push({px, py});
    }
    const int dxs[] = {1, -1, 0, 0};
    const int dys[] = {0, 0, 1, -1};
    while (!q.empty()) {
        auto [x, y] = q.front(); q.pop();
        for (int i = 0; i < 4; ++i) {
            int nx = x + dxs[i];
            int ny = y + dys[i];
            if (ny < 0 || ny >= rows) continue;
            int wx = m.wrapX(nx, ny);
            if (wx < 0 || wx >= cols) continue;
            if (vis[ny][wx]) continue;
            if (m.canEnter(wx, ny, passDoors)) {
                vis[ny][wx] = true;
                q.push({wx, ny});
            }
        }
    }
    return vis;
}
} // namespace

PAC_TEST(Level, playerReachabilityAllLevelFiles) {
    const std::filesystem::path levelsDir = std::filesystem::path(PACMAN_LEVELS_DIR);
    for (const auto& entry : std::filesystem::directory_iterator(levelsDir)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".txt") continue;
        Level level;
        std::string error;
        CHECK(Level::loadFile(entry.path().string(), level, &error));
        if (!level.isValid()) {
            pactest::reportFailure(__FILE__, __LINE__, entry.path().string() + " failed: " + error);
            continue;
        }
        auto vis = reachability(level, false);
        // check every pellet is reachable
        for (int y = 0; y < level.rows(); ++y) {
            for (int x = 0; x < level.cols(); ++x) {
                Tile t = level.tileAt(x, y);
                if (hasPellet(t) || t == Tile::Fruit) {
                    if (!vis[y][x]) {
                        pactest::reportFailure(__FILE__, __LINE__, entry.path().string() + " has unreachable item at " + std::to_string(x) + "," + std::to_string(y));
                    }
                }
                if (t == Tile::Door) {
                    // door itself is blocked to player normally
                }
            }
        }
        // player should not reach inside house interior behind door? 
        // simple check: tiles marked as ' ' inside house bounds in known layouts? 
        // but we just ensure structure valid; core invariant is no unreachable pellets
    }
}
