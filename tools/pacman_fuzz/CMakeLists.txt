cmake_minimum_required(VERSION 3.16)
project(pacman_fuzz LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(pacman_fuzz
    main.cpp
)

target_link_libraries(pacman_fuzz PRIVATE pacman_core)

target_include_directories(pacman_fuzz PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/core/include
)

target_compile_definitions(pacman_fuzz PRIVATE
    PACMAN_LEVELS_DIR="${CMAKE_SOURCE_DIR}/levels"
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(pacman_fuzz PRIVATE -Wall -Wextra -Wpedantic)
endif()
