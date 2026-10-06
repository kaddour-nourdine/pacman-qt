cmake_minimum_required(VERSION 3.16)
project(ai_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ai_demo
    ai_demo.cpp
)

target_link_libraries(ai_demo PRIVATE pacman_core)

target_include_directories(ai_demo PRIVATE
    ${CMAKE_SOURCE_DIR}/core/include
)

target_compile_definitions(ai_demo PRIVATE
    PACMAN_LEVELS_DIR="${CMAKE_SOURCE_DIR}/levels"
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ai_demo PRIVATE -Wall -Wextra -Wpedantic)
endif()
