#!/bin/bash

# Android build script for pacman-qt
# Usage: ./build-android.sh [clean|debug|release]

set -e

# Configuration
BUILD_DIR="build-android"

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

# Gradle needs a full JDK (javac). The default java-25-openjdk is JRE-only.
if [ -z "$JAVA_HOME" ] && [ ! -x /usr/lib/jvm/java-25-openjdk/bin/javac ]; then
    if [ -x /usr/lib/jvm/java-27/bin/javac ]; then
        export JAVA_HOME=/usr/lib/jvm/java-27
        echo "Using JDK from $JAVA_HOME"
    fi
fi

# Add Android tools to PATH
export PATH=$PATH:$ANDROID_HOME/cmdline-tools/latest/bin
export PATH=$PATH:$ANDROID_HOME/platform-tools

# Check prerequisites
function check_prereq() {
    if ! command -v $1 &> /dev/null; then
        echo "Error: $1 not found. Please install Android SDK/NDK."
        exit 1
    fi
}

check_prereq "sdkmanager"
check_prereq "adb"

if [ ! -d "$ANDROID_NDK_ROOT" ]; then
    echo "Error: Android NDK not found at $ANDROID_NDK_ROOT"
    exit 1
fi

if [ ! -f "$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake" ]; then
    echo "Error: Android NDK CMake toolchain not found"
    exit 1
fi

# Parse arguments
BUILD_TYPE="Release"
CLEAN=false

for arg in "$@"; do
    case "$arg" in
        clean)
            CLEAN=true
            ;;
        debug)
            BUILD_TYPE="Debug"
            ;;
        release)
            BUILD_TYPE="Release"
            ;;
        *)
            echo "Unknown argument: $arg"
            echo "Usage: $0 [clean|debug|release]"
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

# Set Qt path for Android - check multiple locations
QT_ANDROID_ROOT=""

# Check common Qt Android installation paths
if [ -d "$HOME/Qt/6.12.0/android_arm64_v8a" ]; then
    QT_ANDROID_ROOT="$HOME/Qt/6.12.0/android_arm64_v8a"
elif [ -d "$HOME/Qt/6.11.2/android_arm64_v8a" ]; then
    QT_ANDROID_ROOT="$HOME/Qt/6.11.2/android_arm64_v8a"
elif [ -d "/opt/Qt/6.12.0/android_arm64_v8a" ]; then
    QT_ANDROID_ROOT="/opt/Qt/6.12.0/android_arm64_v8a"
elif [ -d "/opt/Qt/6.11.2/android_arm64_v8a" ]; then
    QT_ANDROID_ROOT="/opt/Qt/6.11.2/android_arm64_v8a"
fi

if [ -z "$QT_ANDROID_ROOT" ] || [ ! -d "$QT_ANDROID_ROOT" ]; then
    echo "Error: Qt 6 for Android not found!"
    echo "Searched in:"
    echo "  $HOME/Qt/6.12.0/android_arm64_v8a"
    echo "  $HOME/Qt/6.11.2/android_arm64_v8a"
    echo "  /opt/Qt/6.12.0/android_arm64_v8a"
    echo "  /opt/Qt/6.11.2/android_arm64_v8a"
    echo ""
    echo "Please install Qt 6 for Android first."
    exit 1
fi

echo "Using Qt Android: $QT_ANDROID_ROOT"

# Configure with CMake using NDK's built-in toolchain
echo "Configuring for Android (ABI: arm64-v8a, Platform: android-34)..."
cmake -S .. -B . \
    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-34 \
    -DQt6_DIR="$QT_ANDROID_ROOT/lib/cmake/Qt6" \
    -DCMAKE_PREFIX_PATH="$QT_ANDROID_ROOT" \
    -DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" \
    -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DPACMAN_BUILD_APP=ON \
    -DPACMAN_BUILD_TESTS=OFF \
    -DPACMAN_BUILD_TOOLS=OFF

# Build
echo "Building..."
cmake --build . -j$(nproc)

APK_DIR="app/android-build/build/outputs/apk/release"
UNSIGNED_APK="$APK_DIR/android-build-release-unsigned.apk"
SIGNED_APK="$APK_DIR/pacman_app-signed.apk"
KEYSTORE="${PACMAN_KEYSTORE:-$HOME/.android/debug.keystore}"
KEYSTORE_PASS="${PACMAN_KEYSTORE_PASS:-android}"
KEY_ALIAS="${PACMAN_KEY_ALIAS:-androiddebugkey}"
KEY_PASS="${PACMAN_KEY_PASS:-android}"

# Sign the APK. Defaults to the shared debug keystore so a fresh checkout
# still produces an installable APK. For a release build, point these at a
# real key created with keytool (see ANDROID.md "Release signing"):
#   export PACMAN_KEYSTORE=/path/to/release.keystore
#   export PACMAN_KEYSTORE_PASS=...
#   export PACMAN_KEY_ALIAS=pacman
#   export PACMAN_KEY_PASS=...
KEYTOOL="keytool"
if ! command -v keytool &> /dev/null && [ -n "$JAVA_HOME" ] && [ -x "$JAVA_HOME/bin/keytool" ]; then
    KEYTOOL="$JAVA_HOME/bin/keytool"
fi

if [ -f "$UNSIGNED_APK" ]; then
    if [ -z "$PACMAN_KEYSTORE" ] && [ ! -f "$KEYSTORE" ] && command -v "$KEYTOOL" &> /dev/null; then
        echo "Creating debug keystore at $KEYSTORE ..."
        mkdir -p "$HOME/.android"
        "$KEYTOOL" -genkeypair -keystore "$KEYSTORE" -storepass "$KEYSTORE_PASS" -keypass "$KEY_PASS" \
            -alias "$KEY_ALIAS" -keyalg RSA -keysize 2048 -validity 10000 \
            -dname "CN=Android Debug,O=Android,C=US"
    fi
    APKSIGNER=$(ls "$ANDROID_HOME"/build-tools/*/apksigner 2>/dev/null | sort -V | tail -1)
    if [ -n "$APKSIGNER" ]; then
        echo "Signing APK with $KEYSTORE ..."
        "$APKSIGNER" sign --ks "$KEYSTORE" --ks-pass "pass:$KEYSTORE_PASS" --key-pass "pass:$KEY_PASS" \
            --ks-key-alias "$KEY_ALIAS" --out "$SIGNED_APK" "$UNSIGNED_APK" 2>/dev/null
    fi
fi

echo "Build successful!"
echo ""
if [ -f "$SIGNED_APK" ]; then
    echo "Signed APK ready:"
    echo "  $SIGNED_APK"
    echo ""
    echo "To install on a connected device/emulator:"
    echo "  adb install -r \"$SIGNED_APK\""
else
    echo "APK (unsigned):"
    echo "  $UNSIGNED_APK"
fi
