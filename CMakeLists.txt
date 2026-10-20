cmake_minimum_required(VERSION 3.16)

project(pacman-qt
    VERSION 0.1.0
    DESCRIPTION "Pac-Man clone for Linux and Android with a split model/view architecture"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

option(PACMAN_BUILD_TESTS "Build the core engine test suite" ON)

add_subdirectory(core)

option(PACMAN_BUILD_TOOLS "Build tools" ON)
if(PACMAN_BUILD_TOOLS)
    add_subdirectory(tools)
endif()

option(PACMAN_BUILD_APP "Build the Qt Quick application" OFF)
if(PACMAN_BUILD_APP)
    # Android-specific configuration
    if(ANDROID)
        message(STATUS "Configuring for Android build")
        
        # Set Android-specific compiler flags
        add_compile_definitions(ANDROID)
        
        # Ensure position-independent code
        set(CMAKE_POSITION_INDEPENDENT_CODE ON)
        
        # Android-specific warnings suppression for Clang
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            add_compile_options(
                -Wno-unused-command-line-argument
                -Wno-expansion-to-defined
            )
        endif()
    endif()
    
    add_subdirectory(app)
endif()

if(PACMAN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
