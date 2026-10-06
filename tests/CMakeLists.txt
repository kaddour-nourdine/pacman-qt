add_executable(pacman_tests
    TestMain.cpp
    TestEngine.cpp
    TestGame.cpp
    TestGhost.cpp
    TestLevel.cpp
    TestMaze.cpp
    TestPlayer.cpp
    TestPrimitives.cpp
)

target_link_libraries(pacman_tests PRIVATE pacman_core)

target_include_directories(pacman_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(pacman_tests PRIVATE
    PACMAN_LEVELS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../levels"
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(pacman_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

foreach(suite Direction Vec2 Tile Level Maze Player Ghost Game Engine ModeSchedule)
    add_test(NAME ${suite} COMMAND pacman_tests ${suite})
endforeach()