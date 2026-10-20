#!/bin/bash

# Android build script for pacman-qt CORE library only
# This builds just the core library without Qt to verify Android toolchain works
# Usage: ./build-android-core.sh [clean]

set -e

# Configuration
BUILD_DIR="build-android-core"

# Set Android environment
if [ -z "$ANDROID_HOME" ]; then
    export ANDROID_HOME=$HOME/Android
fi

if [ -z "$ANDROID_SDK_ROOT" ]; then
    export ANDROID_SDK_ROOT=$ANDROID_HOME
fi

if [ -z "$ANDROID_NDK_ROOT" ]; then
    export ANDROID_NDK_ROOT=$ANDROID_HOME/ndk/26.2.11394342
fi

# Add Android tools to PATH
export PATH=$PATH:$ANDROID_HOME/cmdline-tools/latest/bin
export PATH=$PATH:$ANDROID_HOME/platform-tools

# Check prerequisites
if [ ! -d "$ANDROID_NDK_ROOT" ]; then
    echo "Error: Android NDK not found at $ANDROID_NDK_ROOT"
    exit 1
fi

if [ ! -f "$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake" ]; then
    echo "Error: Android NDK CMake toolchain not found"
    exit 1
fi

# Parse arguments
CLEAN=false

for arg in "$@"; do
    case "$arg" in
        clean)
            CLEAN=true
            ;;
        *)
            echo "Unknown argument: $arg"
            echo "Usage: $0 [clean]"
            exit 1
            ;;
    esac
done

# Clean build directory if requested
if [ "$CLEAN" = true ]; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake using NDK's built-in toolchain
echo "Configuring core library for Android..."
cmake -S .. -B . \
    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-34 \
    -DCMAKE_BUILD_TYPE=Release \
    -DPACMAN_BUILD_APP=OFF \
    -DPACMAN_BUILD_TESTS=ON \
    -DPACMAN_BUILD_TOOLS=OFF

# Build
echo "Building core library..."
cmake --build . -j$(nproc)

# Check if build succeeded
if [ $? -eq 0 ]; then
    echo ""
    echo "✓ Core library built successfully for Android!"
    echo ""
    echo "Next steps:"
    echo "1. Install Qt 6 Android components"
    echo "2. Run ./build-android.sh to build the full app"
    echo ""
    echo "To test the core library:"
    echo "  ls -la libpacman_core.a"
else
    echo "Build failed!"
    exit 1
fi
