cmake_minimum_required(VERSION 3.16)
project(pacman_bench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(pacman_bench
    pacman_bench.cpp
)

target_link_libraries(pacman_bench PRIVATE pacman_core)

target_include_directories(pacman_bench PRIVATE
    ${CMAKE_SOURCE_DIR}/core/include
)

target_compile_definitions(pacman_bench PRIVATE
    PACMAN_LEVELS_DIR="${CMAKE_SOURCE_DIR}/levels"
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(pacman_bench PRIVATE -Wall -Wextra -Wpedantic)
endif()
