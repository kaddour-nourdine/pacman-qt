# Android Build Instructions

This directory contains Android-specific configuration for building pacman-qt on Android.

## What is actually wired into the build

| Path | Used? | What it does |
| --- | --- | --- |
| `package/` | **yes** | `QT_ANDROID_PACKAGE_SOURCE_DIR` of the `pacman_app` target. Contributes `res/` — the launcher icon (adaptive for API 26+, legacy PNGs for older versions). |
| `icon/` | source only | The artwork the launcher bitmaps are generated from, plus `generate-icons.sh`. |
| `AndroidManifest.xml`, `CMakeLists.txt`, `src/` | no | Reference material from an early Qt 5 attempt. Qt's own template generates the real manifest and activity. |

## Prerequisites

1. **Android SDK** - Installed via `sdkmanager`
2. **Android NDK** - Version 26.2.11394342 or compatible
3. **Qt 6** - With Android support (qt6-qtbase-android, etc.)
4. **Java JDK** - For building Android APKs

## Environment Setup

Add these to your `~/.bashrc` or `~/.zshrc`:

```bash
export ANDROID_HOME=~/Android
export ANDROID_SDK_ROOT=~/Android
export ANDROID_NDK_ROOT=~/Android/ndk/26.2.11394342
export PATH=$PATH:$ANDROID_HOME/cmdline-tools/latest/bin
export PATH=$PATH:$ANDROID_HOME/platform-tools
export PATH=$PATH:$ANDROID_HOME/emulator
```

Then run:
```bash
source ~/.bashrc
```

## Building for Android

### Method 1: Using the build script (recommended)

```bash
# From the repository root — configure + build + APK + sign
./build-android.sh
```

See `../ANDROID.md` for details, prerequisites and the equivalent manual CMake
invocation (it uses the NDK's own toolchain file plus
`-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH`).

### Method 2: Using Qt Creator

1. Open the project in Qt Creator
2. Select the Android kit
3. Configure the build settings to use the Android NDK
4. Build and deploy to device/emulator

## Android-Specific Features

### Launcher Icon
- Adaptive icon (API 26+): `package/res/drawable/ic_launcher_foreground.xml` (a vector
  Pac-Man) on arcade black
- Legacy bitmaps for older versions, generated from `icon/pacman.svg`:
  `bash icon/generate-icons.sh` (needs inkscape or rsvg-convert)
- The launcher label is set with `QT_ANDROID_APP_NAME` ("Pac-Man")

### Touch Input
- Swipe left/right/up/down to change direction
- Tap to start/pause the game

### Lifecycle
- Game pauses when app goes to background
- Game resumes when app comes to foreground
- No frame spikes on resume (Engine handles this)

### UI Adaptations
- Landscape-only orientation
- Safe area insets for notched devices
- Immersive mode (full-screen)
- Back button = pause menu

## Deployment

### To Emulator

```bash
# Start emulator
emulator -avd <your-avd-name> &

# Install APK
adb install -r build-android/app/pacman_app.apk
```

### To Physical Device

```bash
# Enable USB debugging on device
# Connect via USB

# Install APK
adb install -r build-android/app/pacman_app.apk
```

## Troubleshooting

### "NDK not found"
Ensure `ANDROID_NDK_ROOT` is set correctly and points to the NDK directory.

### "SDK not found"
Ensure `ANDROID_SDK_ROOT` is set correctly.

### "Qt Android components missing"
Install Qt Android packages:
```bash
sudo dnf install qt6-qtbase-android qt6-qtdeclarative-android
```

### Build fails with linker errors
Ensure the NDK version matches what Qt was built with. Try NDK 26.x for Qt 6.11.

## Testing

Run on device:
```bash
adb shell am start -n org.pacman.qt/.MainActivity
```

Check logs:
```bash
adb logcat | grep -i pacman
```
