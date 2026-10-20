# Android-specific CMake configuration for pacman-qt
# This file is included when building for Android

cmake_minimum_required(VERSION 3.16)

# Set Android-specific compiler flags
if(ANDROID)
    # Add Android-specific definitions
    target_compile_definitions(pacman_core PRIVATE ANDROID)
    
    # Set position-independent code
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    
    # Android-specific warnings
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(pacman_core PRIVATE
            -Wno-unused-command-line-argument
            -Wno-expansion-to-defined
        )
    endif()
endif()
