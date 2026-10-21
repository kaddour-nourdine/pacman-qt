add_executable(pacman_tests
    TestMain.cpp
    TestDeterminism.cpp
    TestEngine.cpp
    TestGame.cpp
    TestGhost.cpp
    TestLevel.cpp
    TestLevelManager.cpp
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

foreach(suite Direction Vec2 Tile Level LevelManager Maze Player Ghost Game Engine ModeSchedule Determinism)
    add_test(NAME ${suite} COMMAND pacman_tests ${suite})
endforeach()

# The level parser is fuzzed: random headers and maps, plus mutations of the
# levels that ship with the game. The tool lives in tools/pacman_fuzz and is
# only there when the tools are built.
if(TARGET pacman_fuzz)
    add_test(NAME LevelFuzz COMMAND pacman_fuzz --iterations 20000 --seed 20240521)
    set_tests_properties(LevelFuzz PROPERTIES TIMEOUT 300)
endif()

# The installed package has to work: install pacman_core into a temporary
# prefix and build a project outside this tree against it.
add_test(NAME InstallCheck
    COMMAND ${CMAKE_COMMAND}
        -DBUILD_DIR=${CMAKE_BINARY_DIR}
        -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/install_check.cmake
)
set_tests_properties(InstallCheck PROPERTIES TIMEOUT 300)
add_executable(pacman_tests_reach TestReachability.cpp TestMain.cpp)
target_link_libraries(pacman_tests_reach PRIVATE pacman_core)
target_include_directories(pacman_tests_reach PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(pacman_tests_reach PRIVATE
    PACMAN_LEVELS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../levels"
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(pacman_tests_reach PRIVATE -Wall -Wextra -Wpedantic)
endif()

option(PACMAN_BUILD_QT_TESTS "Build Qt view tests" OFF)
if(PACMAN_BUILD_QT_TESTS AND PACMAN_BUILD_APP)
    # Multimedia is needed here as well: pacman_view (and therefore the
    # test binary) links against it, and CMake wants the component to be
    # visible in this scope.
    find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 Test Multimedia)
    qt_add_executable(pacman_qttest
        test_view.cpp
    )
    set_target_properties(pacman_qttest PROPERTIES AUTOMOC ON)
    target_include_directories(pacman_qttest PRIVATE
        ${CMAKE_SOURCE_DIR}/app
        ${CMAKE_SOURCE_DIR}/core/include
    )
    target_link_libraries(pacman_qttest PRIVATE
        pacman_view
        Qt6::Quick
        Qt6::Test
    )
    qt_import_qml_plugins(pacman_qttest)
    add_test(NAME QtView COMMAND pacman_qttest)
    set_tests_properties(QtView PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()
