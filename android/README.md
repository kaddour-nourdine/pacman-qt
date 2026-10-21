# Android Build Instructions

This directory contains Android-specific configuration for building pacman-qt on Android.

## What is actually wired into the build

| Path | Used? | What it does |
| --- | --- | --- |
| `package/` | **yes** | `QT_ANDROID_PACKAGE_SOURCE_DIR` of the `pacman_app` target. Contributes the manifest, the activity (`package/src/org/pacman/qt/MainActivity.java`) and `res/` — the launcher icon (adaptive for API 26+, legacy PNGs for older versions). |
| `icon/` | source only | The artwork the launcher bitmaps are generated from, plus `generate-icons.sh`. |
| `AndroidManifest.xml`, `CMakeLists.txt`, `deploy.json` | no | Reference material from an early Qt 5 attempt. Qt's own template generates the real manifest, and the activity now lives in `package/src/`. |

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
- Game pauses when app goes to background (`Qt::ApplicationSuspended`): the
  tick timer stops and the model is frozen
- Game resumes when app comes to foreground; the time spent in the background
  is thrown away, so there is no frame spike on resume

### UI Adaptations
- Landscape-only orientation (`sensorLandscape`: both landscape ways)
- Safe area insets for notched devices (`QWindow::safeAreaMargins()`)
- Immersive mode (full-screen, the system bars come back on a swipe)
- Back button = pause menu (the activity forwards it to the game through JNI,
  see `package/src/org/pacman/qt/MainActivity.java`)

## The Activity

`package/src/org/pacman/qt/MainActivity.java` extends Qt's `QtActivity` and
adds three things:

1. **Orientation** — the game is locked to landscape.
2. **Immersive mode** — the status and navigation bars are hidden; a swipe
   from the edge brings them back.
3. **The back button** — Android hands the back press to the activity, and
   Qt's own input delegate swallows it (an `OnBackInvokedCallback` on API 33+,
   a consumed key event on older versions). The activity intercepts it and
   calls `nativeBackPressed()`, a JNI function in `app/AndroidBridge.cpp`
   that opens the pause menu.

The manifest in `package/AndroidManifest.xml` replaces the generated one, so
it has to keep everything Qt needs: the `QtApplication`, the
`android.app.lib_name` meta data and the `FileProvider`.

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
