# Android Build Status - Phase 5

**Date:** 2026-10-20
**Status:** ✅ **Full app builds, APK produced and signed**

---

## Achievement

**The complete pacman-qt Qt Quick app cross-compiles for Android ARM64 and packages into a
signed APK.**

```
build-android/app/android-build/build/outputs/apk/release/
  ├── android-build-release-unsigned.apk   (67.8 MB)
  └── pacman_app-signed.apk                (67.9 MB, signed, installable)
```

The APK contains:
- `lib/arm64-v8a/libpacman_app_arm64-v8a.so` — our app (1.26 MB)
- 32 Qt 6.12.0 libraries for arm64-v8a (Quick, Qml, QuickControls2, …)
- platform plugin + image format plugins
- `assets/android_rcc_bundle.rcc` — QML module and resources

---

## How it builds

```bash
./build-android.sh        # configure + build + APK + sign
adb install -r build-android/app/android-build/build/outputs/apk/release/pacman_app-signed.apk
```

The script:
1. configures CMake with the NDK's own toolchain file
2. builds `pacman_core`, `pacman_view`, `pacman_app`
3. runs `androiddeployqt` + Gradle to produce the APK
4. signs it with `~/.android/debug.keystore` (auto-created) so `adb install` works

---

## Root causes that had to be fixed (for future reference)

### 1. NDK toolchain re-roots every `find_package` into the sysroot

`android-legacy.toolchain.cmake` sets `CMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY`, so all
Qt search paths are rewritten under the NDK sysroot where Qt does not exist.

**Fix:** pass `-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH` on the CMake command line
(the toolchain guards its own setting with `if(NOT …)`, so the command line wins).

### 2. Qt installation was damaged (restored)

A previous session's `rm -rf` had deleted `lib/cmake/{Qt6Core,Qt6Gui,Qt6Quick,Qt6QuickControls2}`
from `~/Qt/6.12.0/android_arm64_v8a/`. Restored by copying from the intact `android_armv7`
kit and rewriting `armeabi-v7a` → `arm64-v8a` in `*Targets*.cmake` only.

The copied `*ConfigVersionImpl.cmake` files also hardcoded the 32-bit pointer size
(`STREQUAL "4"`), which made `find_package(Qt6 … 6.12.0)` reject the kit. Changed to
`STREQUAL "8"` in all 12 restored files (Core, Gui, Quick, QuickControls2 + plugins).

### 3. `ANDROID_SDK_ROOT` must be a CMake variable

Qt's `Qt6AndroidMacros.cmake` reads `${ANDROID_SDK_ROOT}/build-tools` as a **CMake
variable**, not an environment variable. The script now passes
`-DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT"`.

### 4. Gradle needs a full JDK

Default `java-25-openjdk` is JRE-only (no `javac`), so Gradle's toolchain detection
failed. The script exports `JAVA_HOME=/usr/lib/jvm/java-27` (a full JDK).

### 5. `androidx.core:core` requires compileSdk ≥ 36

Qt's Gradle template bundles `androidx.core:core:1.17.0`, which demands
`compileSdk >= 36`, while `ANDROID_PLATFORM=android-34` (the NDK API level) fed
`compileSdkVersion android-34` into `gradle.properties`.

**Fix:** keep the NDK at API 34 and only raise the Gradle-side SDK via Qt's target
property in `app/CMakeLists.txt`:

```cmake
if(ANDROID)
    set_target_properties(pacman_app PROPERTIES
        QT_ANDROID_COMPILE_SDK_VERSION "android-36")
endif()
```

Prerequisites installed: `platforms;android-36`, `build-tools;36.0.0`, SDK licenses
accepted (`yes | sdkmanager --licenses`).

### 6. Android NDK libc++ has no float `from_chars`

`core/src/Level.cpp` `parseFloat` first switched to `std::strtof`, which made it
locale-dependent and re-broke `Level.parsesIndependentlyOfNumericLocale` (Qt installs
`fr_FR`, comma decimals). Final state: a classic-locale `std::istringstream` — portable,
locale-independent, and guarded by the same test.

---

## Phase 5 Completion

| Task | Status | Notes |
|------|--------|-------|
| Android SDK/NDK toolchain | ✅ Done | NDK 26.2.11394342, SDK platforms 34/36, build-tools 36 |
| Qt 6.12.0 for Android install | ✅ Done | via Qt Online Installer (gcc_64/Android kits) |
| Qt Android CMake integration | ✅ **Done** | `MODE_PACKAGE=BOTH` + restored kit files |
| Core library compilation | ✅ Done | `libpacman_core.a` for arm64-v8a |
| Full app build | ✅ **Done** | `pacman_core`, `pacman_view`, `pacman_app` |
| APK generation | ✅ **Done** | `android-build-release-unsigned.apk` |
| APK signing | ✅ Done | debug keystore, `apksigner verify` passes |
| Install on a device/emulator | ⏳ Pending | no device attached at build time |
| Device runtime testing | ⏳ Pending | see below |

**Overall: build pipeline 100% complete; device testing pending.**

---

## Known runtime gaps before first launch

1. ~~**Levels directory is a build-host path.**~~ **Resolved**: `levels/*.txt` is now
   embedded as Qt resources (`pacman_levels` in `app/CMakeLists.txt`), and when the
   compiled-in `PACMAN_LEVELS_DIR` does not exist `GameController::materializeBundledLevels()`
   copies them into `QStandardPaths::AppDataLocation` for `LevelManager::loadDirectory`
   (core stays Qt-free). Covered by `ViewTest.testBundledLevelsAreShipped`.
2. **Touch input**: swipe-to-turn with a dead zone not implemented yet.
3. **Lifecycle**: suspend/resume pause handling untested on device.
4. **Orientation / safe areas / back button**: not handled yet (see `todo.md` Phase 5).
5. **Signing**: currently a shared debug keystore; release signing needs a real key.

---

## Files involved

```
build-android.sh                    # full pipeline (configure → build → APK → sign)
build-android-core.sh               # core-only build (kept as a fast smoke test)
app/CMakeLists.txt                  # + QT_ANDROID_COMPILE_SDK_VERSION property
core/src/Level.cpp                  # classic-locale istringstream float parsing (portable)
android/                            # manifest/activity reference material (Qt templates drive the real build)
~/Qt/6.12.0/android_arm64_v8a/      # Qt kit (4 cmake dirs restored from armv7 + bitness fix)
```

---

**Status:** ✅ Build pipeline working end-to-end
**Next Action:** attach a device/emulator, `adb install` the signed APK, fix level
bundling, then work through the device-testing items in `todo.md` Phase 5.
