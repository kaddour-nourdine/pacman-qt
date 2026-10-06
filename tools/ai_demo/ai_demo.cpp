#include <iostream>
#include <string>

#include "pacman/Engine.hpp"
#include "pacman/Game.hpp"
#include "pacman/Level.hpp"

using namespace pac;

int main() {
    Level level;
    std::string err;
    if (!Level::loadFile(std::string(PACMAN_LEVELS_DIR) + "/level1.txt", level, &err)) return 1;
    Game game(level);
    Engine engine(game);
    engine.setTimeScale(5.0f);
    game.start();
    for (int i = 0; i < 10000; ++i) {
        engine.update(1.0 / 120.0);
        if (game.isOver()) { std::cout << "OVER\n"; return 0; }
    }
    std::cout << "OK\n";
    return 0;
}
