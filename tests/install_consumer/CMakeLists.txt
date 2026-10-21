cmake_minimum_required(VERSION 3.16)
project(pacman_core_consumer LANGUAGES CXX)

# The whole point of the export rules: an outside project finds the package
# and links the target.
find_package(pacman_core REQUIRED)

add_executable(pacman_core_consumer main.cpp)
target_link_libraries(pacman_core_consumer PRIVATE pacman::core)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(pacman_core_consumer PRIVATE -Wall -Wextra -Wpedantic)
endif()
