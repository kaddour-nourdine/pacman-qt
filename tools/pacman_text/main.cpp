#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "pacman/Engine.hpp"
#include "pacman/Game.hpp"
#include "pacman/Level.hpp"
#include "pacman/LevelManager.hpp"

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

using namespace pac;

#ifndef _WIN32
static struct termios oldt;
static bool termios_saved = false;

static void restore_terminal() {
    if (termios_saved) {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    }
}

static void enable_raw_terminal() {
    struct termios newt;
    tcgetattr(STDIN_FILENO, &oldt);
    termios_saved = true;
    std::atexit(restore_terminal);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
}

static bool kbhit() {
    int byteswaiting = 0;
    ioctl(STDIN_FILENO, FIONREAD, &byteswaiting);
    return byteswaiting > 0;
}

static char getch() {
    char buf = 0;
    if (read(STDIN_FILENO, &buf, 1) <= 0) {
        return 0;
    }
    return buf;
}
#endif

static std::string levels_dir() {
    return PACMAN_LEVELS_DIR;
}

static void draw_maze(const Game& game) {
    const Maze& maze = game.maze();
    const Player& player = game.player();
    const auto& ghosts = game.ghosts();
    const int cols = maze.cols();
    const int rows = maze.rows();

    // build a 2D map of what to draw
    std::vector<std::vector<char>> grid(rows, std::vector<char>(cols, ' '));
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            const Tile t = maze.tileAt(x, y);
            switch (t) {
            case Tile::Wall:
                grid[y][x] = '#';
                break;
            case Tile::Pellet:
                grid[y][x] = '.';
                break;
            case Tile::PowerPellet:
                grid[y][x] = 'o';
                break;
            case Tile::Door:
                grid[y][x] = '-';
                break;
            case Tile::Tunnel:
                grid[y][x] = ' ';
                break;
            case Tile::Fruit:
                grid[y][x] = 'F';
                break;
            case Tile::Empty:
                grid[y][x] = ' ';
                break;
            }
        }
    }

    // draw ghosts
    for (const auto& ghost : ghosts) {
        const int gx = ghost.tileX();
        const int gy = ghost.tileY();
        if (gx < 0 || gx >= cols || gy < 0 || gy >= rows) {
            continue;
        }
        char c = 'G';
        switch (ghost.id()) {
        case GhostId::Blinky:
            c = 'B';
            break;
        case GhostId::Pinky:
            c = 'P';
            break;
        case GhostId::Inky:
            c = 'I';
            break;
        case GhostId::Clyde:
            c = 'C';
            break;
        }
        if (ghost.isEyes()) {
            c = 'E';
        } else if (ghost.isEdible()) {
            c = 'v';
        }
        grid[gy][gx] = c;
    }

    // draw player
    {
        const int px = player.tileX();
        const int py = player.tileY();
        if (px >= 0 && px < cols && py >= 0 && py < rows) {
            grid[py][px] = '@';
        }
    }

    // print
    std::cout << "\033[2J\033[H";
    std::cout << "L:" << game.levelNumber() << " [" << game.levelName() << "]"
              << " Score:" << game.score() << " Lives:" << game.lives()
              << " Pellets:" << game.pelletsRemaining() << " State:" << (int)game.state() << "\n";
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            std::cout << grid[y][x];
        }
        std::cout << "\n";
    }
}

int main(int argc, char** argv) {
    std::string levelPath;
    if (argc >= 2) {
        levelPath = argv[1];
    } else {
        levelPath = levels_dir() + "/level1.txt";
    }

    LevelManager lm;
    std::string error;
    if (!lm.loadFile(levelPath, &error) && error.find("cannot open") != std::string::npos) {
        // try loading from levels dir by name
        lm.loadDirectory(levels_dir(), &error);
    }
    if (lm.isEmpty()) {
        std::cerr << "Failed to load level(s): " << error << "\n";
        return 1;
    }

    Game game(lm);
    Engine engine(game);
    engine.setTimeScale(1.0f);

#ifndef _WIN32
    enable_raw_terminal();
#endif

    game.start();

    while (!game.isOver()) {
#ifndef _WIN32
        if (kbhit()) {
            char c = getch();
            switch (c) {
            case 'q':
            case 'Q':
                goto end;
            case '\033':
                if (kbhit()) {
                    char c2 = getch();
                    if (c2 == '[' && kbhit()) {
                        char c3 = getch();
                        switch (c3) {
                        case 'A':
                            game.player().requestDirection(Direction::Up);
                            break;
                        case 'B':
                            game.player().requestDirection(Direction::Down);
                            break;
                        case 'C':
                            game.player().requestDirection(Direction::Right);
                            break;
                        case 'D':
                            game.player().requestDirection(Direction::Left);
                            break;
                        default:
                            break;
                        }
                    }
                }
                break;
            default:
                if (c == 'a') game.player().requestDirection(Direction::Left);
                if (c == 'd') game.player().requestDirection(Direction::Right);
                if (c == 'w') game.player().requestDirection(Direction::Up);
                if (c == 's') game.player().requestDirection(Direction::Down);
                break;
            }
        }
#endif
        engine.update(1.0 / 120.0);
        draw_maze(game);
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }

end:
    draw_maze(game);
    std::cout << "\nGame over. Final score: " << game.score() << "\n";

    return 0;
}
