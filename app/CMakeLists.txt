find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2)

set(CMAKE_AUTOMOC ON)

qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

qt_add_library(pacman_view STATIC
    GameController.cpp
    GameController.hpp
)

qt_add_qml_module(pacman_view
    URI PacMan
    VERSION 1.0
    NO_PLUGIN
    QML_FILES
        Main.qml
)

qt_add_resources(pacman_view "pacman_assets"
    PREFIX "/"
    FILES
        assets/sprites/tiles.svg
        assets/sprites/pacman.svg
        assets/sprites/ghost_red.svg
        assets/sprites/ghost_pink.svg
        assets/sprites/ghost_cyan.svg
        assets/sprites/ghost_orange.svg
        assets/sprites/eyes.svg
        assets/sprites/frightened.svg
        assets/sprites/life.svg
        assets/audio/munch.wav
        assets/audio/power.wav
)

target_link_libraries(pacman_view PUBLIC
    pacman_core
    Qt6::Quick
    Qt6::QuickControls2
)

target_compile_definitions(pacman_view PRIVATE
    PACMAN_LEVELS_DIR="${CMAKE_SOURCE_DIR}/levels"
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(pacman_view PRIVATE -Wall -Wextra -Wpedantic)
endif()

qt_add_executable(pacman_app MANUAL_FINALIZATION main.cpp)

target_link_libraries(pacman_app PRIVATE pacman_view)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(pacman_app PRIVATE -Wall -Wextra -Wpedantic)
endif()

qt_finalize_executable(pacman_app)
