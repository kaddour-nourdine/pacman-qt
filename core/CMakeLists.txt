add_library(pacman_core STATIC
    src/Engine.cpp
    src/Entity.cpp
    src/Ghost.cpp
    src/Game.cpp
    src/Level.cpp
    src/LevelManager.cpp
    src/Maze.cpp
    src/Player.cpp
)

add_library(pacman::core ALIAS pacman_core)

target_include_directories(pacman_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(pacman_core PUBLIC cxx_std_17)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(pacman_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(pacman_core PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)