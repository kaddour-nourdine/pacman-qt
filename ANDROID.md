# Android Development Guide for pacman-qt

This guide covers setting up and building pacman-qt for Android (Phase 5).
The full pipeline — cross-compile, APK packaging, signing — is documented here and
works as of 2026-10-20 (see `ANDROID_BUILD_STATUS.md` for the technical history).

## Prerequisites

### 1. Android SDK and NDK

```bash
# Install dependencies (JDK with javac — Gradle needs it)
sudo dnf install -y java-latest-openjdk-devel unzip zip

# Download Android Command Line Tools
mkdir -p ~/Android/cmdline-tools
wget -O ~/Android/cmdline-tools/tools.zip https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip
unzip ~/Android/cmdline-tools/tools.zip -d ~/Android/cmdline-tools/
rm ~/Android/cmdline-tools/tools.zip

# Install SDK packages
export ANDROID_HOME=~/Android
export ANDROID_SDK_ROOT=~/Android
export PATH=$PATH:$ANDROID_HOME/cmdline-tools/latest/bin

sdkmanager --install "platform-tools" "platforms;android-34" "platforms;android-36" \
    "build-tools;34.0.0" "build-tools;36.0.0" "ndk;26.2.11394342"

# Accept licenses (required for Gradle's automatic SDK installs)
yes | sdkmanager --licenses
```

Notes:
- `platforms;android-36` + `build-tools;36.0.0` are required by the Gradle side
  (Qt's template bundles `androidx.core:core`, which needs compileSdk ≥ 36).
- The C++/NDK side still compiles against API 34 (`ANDROID_PLATFORM=android-34`),
  which is the maximum supported by NDK 26.

### 2. Qt 6 for Android

Fedora's dnf Qt 6 packages **cannot** cross-compile for Android (no CMake config
files, no Android kits). Install the Android kit with the Qt Online Installer:

1. Download from <https://www.qt.io/download-qt-installer> and log in with a Qt account.
2. During install, select **Qt 6.12.0 → Android iosa64** (arm64-v8a) — the default
   install also brings the host `gcc_64` kit and Android x86/x86_64/armv7 kits,
   which are useful (the armv7 kit doubles as a repair source for damaged files).
3. The kit ends up at:

```
~/Qt/6.12.0/android_arm64_v8a/   # libraries, CMake configs, host tools (qmake, qt-cmake)
```

## Environment Setup

Add to `~/.bashrc` (all optional — `build-android.sh` sets what it needs):

```bash
export ANDROID_HOME=~/Android
export ANDROID_SDK_ROOT=~/Android
export ANDROID_NDK_ROOT=~/Android/ndk/26.2.11394342
export PATH=$PATH:$ANDROID_HOME/cmdline-tools/latest/bin
export PATH=$PATH:$ANDROID_HOME/platform-tools
```

## Building for Android

### Quick Build

```bash
./build-android.sh          # incremental
./build-android.sh clean    # from scratch
./build-android.sh release  # Release build (default)
./build-android.sh debug    # Debug build
```

The script:
1. configures CMake with the **NDK's own toolchain file**
   (`$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake`), ABI `arm64-v8a`,
   `ANDROID_PLATFORM=android-34`
2. passes `-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH` — without it the NDK toolchain
   re-roots every `find_package` into the NDK sysroot and Qt can never be found
3. points CMake at the Qt kit (`Qt6_DIR` + `CMAKE_PREFIX_PATH`) and
   `ANDROID_SDK_ROOT` as a CMake variable
4. picks a full JDK (`JAVA_HOME=/usr/lib/jvm/java-27`) for Gradle
5. builds `pacman_core` → `pacman_view` → `pacman_app`
6. runs `androiddeployqt` + Gradle to produce the APK
7. signs it with `~/.android/debug.keystore` (created on first run)

### Manual Build (reference — same flags as the script)

```bash
mkdir -p build-android
cd build-android

cmake -S .. -B . \
    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-34 \
    -DQt6_DIR=$HOME/Qt/6.12.0/android_arm64_v8a/lib/cmake/Qt6 \
    -DCMAKE_PREFIX_PATH=$HOME/Qt/6.12.0/android_arm64_v8a \
    -DANDROID_SDK_ROOT=$ANDROID_HOME \
    -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH \
    -DCMAKE_BUILD_TYPE=Release \
    -DPACMAN_BUILD_APP=ON \
    -DPACMAN_BUILD_TESTS=OFF \
    -DPACMAN_BUILD_TOOLS=OFF

cmake --build . -j$(nproc)
```

Do **not** use custom toolchain files — earlier experiments
(`android-toolchain.cmake` etc.) were dead ends; the NDK's own file plus the two
`find_package`-related variables above is the working combination.

### Build only the core library

```bash
./build-android-core.sh      # fast smoke test: libpacman_core.a for arm64-v8a
```

## Build Outputs

```
build-android/app/android-build/build/outputs/apk/release/
├── android-build-release-unsigned.apk    # produced by Gradle
└── pacman_app-signed.apk                 # signed, installable (created by build-android.sh)
```

The APK (~68 MB) contains `lib/arm64-v8a/libpacman_app_arm64-v8a.so`, 32 Qt
libraries, the platform/image plugins, and `assets/android_rcc_bundle.rcc`.

## Deploying to Device/Emulator

### Install on a connected device

```bash
adb install -r build-android/app/android-build/build/outputs/apk/release/pacman_app-signed.apk
```

### Create an Android Virtual Device (AVD)

```bash
# List available system images
sdkmanager --list | grep "system-images"

# Install a system image (x86_64 image runs on most desktops; the APK is arm64,
# so prefer an arm64 system image or build with -DANDROID_ABI=x86_64)
sdkmanager "system-images;android-34;google_apis;x86_64"

# Create and start the AVD
avdmanager create avd -n pacman-avd -k "system-images;android-34;google_apis;x86_64" -d pixel_6_pro
emulator -avd pacman-avd -no-snapshot-load &
```

### Run the App

```bash
adb shell monkey -p org.pacman.qt 1      # launch (Qt template activity, no custom intent name)
```

## Known Runtime Gaps (before first device test)

1. **Levels are not bundled yet.** `PACMAN_LEVELS_DIR` compiles in the build-host
   path (`${CMAKE_SOURCE_DIR}/levels`), which does not exist on the phone;
   `GameController` logs a warning and starts with no levels. Bundle `levels/*.txt`
   as Qt resources for Android and load from there.
2. Touch input (swipe-to-turn with dead zone) not implemented in QML yet.
3. Suspend/resume lifecycle handling untested on device.
4. Orientation/safe areas/back button not handled (Qt activity, not the
   reference `android/MainActivity.java`).
5. Signing uses a shared debug keystore — create a real key for releases.

## Android-Specific Code Changes So Far

- `core/src/Level.cpp`: `parseFloat` uses a classic-locale `std::istringstream`
  instead of `std::from_chars` (NDK libc++ has no floating-point `from_chars`)
  and instead of `strtof` (locale-dependent — it broke under Qt's `fr_FR`);
  the `Level.parsesIndependentlyOfNumericLocale` test guards locale behavior.
- `app/CMakeLists.txt`: sets `QT_ANDROID_COMPILE_SDK_VERSION` to `android-36` so
  Gradle's compileSdk satisfies `androidx.core` while the NDK stays at API 34.
- Root `CMakeLists.txt`: `if(ANDROID)` block adds `-DANDROID` and warning
  suppressions for Clang.

## Testing

### Check Logs

```bash
adb logcat | grep -i pacman
```

## Troubleshooting

### `find_package(Qt6 …)` fails / "Could not find a package configuration file"

- Verify `-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH` is on the command line —
  the NDK toolchain forces `ONLY` otherwise and re-roots the search into the
  sysroot. Debug with `-DCMAKE_FIND_ROOT_PATH_DEBUG=ON` or
  `--debug-find-pkg=Qt6Quick`.
- Verify `Qt6_DIR=…/android_arm64_v8a/lib/cmake/Qt6`.

### "Could not locate Android SDK build tools under …/build-tools"

`ANDROID_SDK_ROOT` must be passed as a **CMake variable**
(`-DANDROID_SDK_ROOT=…`), an environment variable alone is not enough. Also make
sure `build-tools;36.0.0` is installed.

### "Toolchain installation … does not provide … [JAVA_COMPILER]"

Gradle picked a JRE-only Java. Point it at a full JDK:
`export JAVA_HOME=/usr/lib/jvm/java-27` (the build script does this automatically).

### Gradle: dependency compiled against android-34, needs ≥ 36

The `QT_ANDROID_COMPILE_SDK_VERSION` property in `app/CMakeLists.txt` must be set
(see above) and `platforms;android-36` installed.

### Version mismatch "Expected platform version 4 …" or `ConfigVersionImpl` errors

Symptom of a damaged Qt kit — the `*ConfigVersionImpl.cmake` files hardcode the
kit's pointer size. The arm64 kit must say `"8"`, never `"4"` (the `"4"` variant
comes from the 32-bit armv7 kit). Repair by copying from
`~/Qt/6.12.0/android_armv7/lib/cmake/…` and fixing `armeabi-v7a` → `arm64-v8a`
in `*Targets*.cmake` only.

### "Building the android package failed"

Run Gradle directly for the full error:

```bash
cd build-android/app/android-build
JAVA_HOME=/usr/lib/jvm/java-27 ./gradlew assembleRelease --info
```

### Reinstalling/repairing Qt

`~/Qt/MaintenanceTool` (headless: `MaintenanceTool --accept-licenses install …`,
commands `li` = list, `se` = search) can modify the installed kits — but note the
repair workflow above is often faster for missing CMake files.

## References

- [Qt for Android Documentation](https://doc.qt.io/qt-6/android.html)
- [Android NDK Documentation](https://developer.android.com/ndk)
- [CMake Android Cross-Compilation](https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html#cross-compiling-for-android)
