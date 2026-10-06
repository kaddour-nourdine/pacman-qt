find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2)

set(CMAKE_AUTOMOC ON)

qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

qt_add_executable(pacman_app MANUAL_FINALIZATION
    main.cpp
    GameController.cpp
    GameController.hpp
)

qt_add_qml_module(pacman_app
    URI PacMan
    VERSION 1.0
    QML_FILES
        Main.qml
)

target_link_libraries(pacman_app PRIVATE
    pacman_core
    Qt6::Quick
    Qt6::QuickControls2
)

target_compile_definitions(pacman_app PRIVATE
    PACMAN_LEVELS_DIR="${CMAKE_SOURCE_DIR}/levels"
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(pacman_app PRIVATE -Wall -Wextra -Wpedantic)
endif()

qt_finalize_executable(pacman_app)
