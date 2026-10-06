#pragma once

#include <string>
#include <vector>

#include "TestFramework.hpp"
#include "pacman/Level.hpp"

namespace pactest {

inline pac::Level makeLevel(const std::vector<std::string>& rows, const char* file, int line) {
    pac::Level level;
    std::string error;
    if (!pac::Level::parse(rows, level, &error)) {
        reportFailure(file, line, "level parse failed: " + error);
    }
    return level;
}

}  // namespace pactest

#define MAKE_LEVEL(...) pactest::makeLevel(std::vector<std::string>{__VA_ARGS__}, __FILE__, __LINE__)

namespace pactest {

inline std::vector<std::string> layoutMap() {
    return {
        "###########",
        "#.1.2.P.3.#",
        "#.#.#.#.#.#",
        "#..o...4..#",
        "#.#.#.#.#.#",
        "#....#....#",
        "###########",
    };
}

inline std::vector<std::string> ringMap() {
    return {
        "###########",
        "_o..P....._",
        "###########",
    };
}

inline std::vector<std::string> houseMap() {
    return {
        "###########",
        "#....P....#",
        "#.###.#.###",
        "#.#-----#.#",
        "#.#123  #.#",
        "#.#######.#",
        "#....4....#",
        "###########",
    };
}

inline std::vector<std::string> junctionMap() {
    return {
        "#####",
        "#P..#",
        "#...#",
        "#...#",
        "#####",
    };
}

inline std::vector<std::string> gameMap() {
    return {
        "#############",
        "#...........#",
        "#.#######...#",
        "#.P#1234#...#",
        "#.#######...#",
        "#.........o.#",
        "#############",
    };
}

inline std::vector<std::string> modeMap() {
    return {
        "name = Modes",
        "playerSpeed = 0.001",
        "ghostSpeed = 0.001",
        "frightenedSpeed = 0.001",
        "tunnelSpeed = 0.001",
        "frightenedSeconds = 1.0",
        "modeSchedule = 1,2",
        "",
        "#############",
        "#...........#",
        "#.#######...#",
        "#.P#1234#...#",
        "#.#######...#",
        "#.........o.#",
        "#############",
    };
}

inline std::vector<std::string> firstRoomMap() {
    return {
        "name = First Room",
        "playerSpeed = 0.001",
        "ghostSpeed = 0.001",
        "frightenedSpeed = 0.001",
        "tunnelSpeed = 0.001",
        "eyesSpeed = 0.001",
        "frightenedSeconds = 1.0",
        "modeSchedule = 5,5",
        "",
        "#######",
        "#P.123#",
        "#.....#",
        "#.o...#",
        "#4....#",
        "#######",
    };
}

inline std::vector<std::string> secondRoomMap() {
    return {
        "name = Second Room",
        "playerSpeed = 0.001",
        "ghostSpeed = 0.001",
        "frightenedSpeed = 0.001",
        "tunnelSpeed = 0.001",
        "eyesSpeed = 0.001",
        "frightenedSeconds = 1.0",
        "modeSchedule = 5,5",
        "",
        "#######",
        "#P..o.#",
        "#.###.#",
        "#1234.#",
        "#.....#",
        "#######",
    };
}

}  // namespace pactest