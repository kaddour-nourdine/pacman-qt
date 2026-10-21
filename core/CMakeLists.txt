include(CMakePackageConfigHelpers)

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
    # The installed target is pacman::core, exactly like the alias above.
    EXPORT_NAME core
)

# ---------------------------------------------------------------------------
# Installation
#
# The library is installed with its headers and a package configuration, so
# that anything outside this tree can use it with
#
#     find_package(pacman_core REQUIRED)
#     target_link_libraries(my_tool PRIVATE pacman::core)
#
# The Android app builds against the sources, but a future tool -- or a
# second project that wants the model -- can use the installed package.
# ---------------------------------------------------------------------------

set(PACMAN_CORE_PUBLIC_HEADERS
    include/pacman/Direction.hpp
    include/pacman/Engine.hpp
    include/pacman/Entity.hpp
    include/pacman/Events.hpp
    include/pacman/Game.hpp
    include/pacman/Ghost.hpp
    include/pacman/Level.hpp
    include/pacman/LevelManager.hpp
    include/pacman/Maze.hpp
    include/pacman/Player.hpp
    include/pacman/Random.hpp
    include/pacman/Tile.hpp
    include/pacman/Vec2.hpp
)

install(TARGETS pacman_core
    EXPORT pacman_coreTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES ${PACMAN_CORE_PUBLIC_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/pacman
)

install(EXPORT pacman_coreTargets
    FILE pacman_coreTargets.cmake
    NAMESPACE pacman::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/pacman_core
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/pacman_coreConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/pacman_coreConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/pacman_core
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/pacman_coreConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/pacman_coreConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/pacman_coreConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/pacman_core
)
