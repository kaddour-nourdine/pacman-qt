// A project outside the pacman-qt tree, built against an installed
// pacman_core. It exists to prove that the export rules work: the headers
// are found, the library is found, and the target pacman::core carries
// everything a consumer needs.
//
// The install check (tests/install_check.cmake) configures and builds this
// project against a temporary prefix and runs the result.

#include <pacman/Level.hpp>

#include <iostream>
#include <string>
#include <vector>

int main() {
    const std::vector<std::string> lines = {
        "name = Consumer", "playerSpeed = 6.0", "",          "#########",
        "#...P...#",       "#.......#",         "#########",
    };

    pac::Level level;
    std::string error;
    if (!pac::Level::parse(lines, level, &error)) {
        std::cerr << "the installed library could not parse a level: " << error << "\n";
        return 1;
    }

    std::cout << "pacman_core works: " << level.cols() << "x" << level.rows() << " map, "
              << level.pelletCount() << " pellets\n";
    return 0;
}
