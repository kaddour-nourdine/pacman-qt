# Android Build Status - Phase 5

**Date:** 2026-10-20 (audio verified 2026-10-21)
**Status:** ✅ **Full app builds, APK produced, signed, installed and launched on a phone — audio verified on the device**

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

### 7. Per-play `QSoundEffect` restarts stalled the whole frame pipeline (on device)

First device test: every pellet eaten stuttered the visible game. Root-caused in
three steps with on-device instrumentation (shipped in commit `bedd1dc`):

1. `QSoundEffect::play()` blocked the **UI thread** 15–40 ms per pellet — the 90 ms
   munch ends between pellets, so each play did a full `AudioTrack` stop/start.
   Tick instrumentation: `tick gap 54–98 ms … ui-cpu 15–40 ms`, in lockstep with
   eating.
2. Moving playback to a worker thread removed the UI block but **not** the stutter:
   `tick gap ~50 ms; work 0 ms` with UI + render threads sleeping together
   (48/48 correlated render-thread gaps) — the restart perturbed the entire
   frame pipeline, not just the calling thread.
3. A no-sound A/B build cut tick gaps from ~50 per game to ~5 → audio confirmed
   as the trigger (and the user felt the difference).

**Fix:** `app/SfxPlayer.{hpp,cpp}` — one `QAudioSink` opened at startup and fed
until exit; both WAVs pre-decoded to float PCM, mixed on the audio callback
thread (4 voices, linear resampling; Int16/Int32/Float/UInt8 formats). Triggering
a sound from the game thread is a mutex + a few writes with a ≥4 ms tripwire log —
nothing starts or stops during gameplay. No audio device = warning, not a failure
(headless tests keep passing).

**Verified on device** (2026-10-21) — but only after §8 below: the mixer was
silent until `bytesAvailable()` was overridden.

---

### 8. The persistent mixer was silent: Qt only pulls from a device that claims data

The mixer of §7 was correct but **produced no sound at all**. On the phone:
`QAudioSink` sat in `IdleState`, `framesWritten` stayed at 0, and nothing was
ever heard — while the desktop showed nothing wrong either (no error, no
warning).

**Root cause** (Qt 6.12 source,
`qtmultimedia/src/multimedia/audio/qaudio_qiodevice_support_p.h`):

```cpp
// pullFromQIODeviceToRingbuffer()
if (device->bytesAvailable() <= 0)   // ← a plain QIODevice reports 0 forever
    return;                          // ← nothing is read, ever
```

`SfxMixer` derives from `QIODevice` and only implements `readData()`. A plain
`QIODevice` has no data of its own, so `bytesAvailable()` returned 0, the pull
loop bailed out immediately, the sink never entered `ActiveState` and the game
was completely silent. The mixer was a *push* device pretending to be a *pull*
device.

**Fix:** override `bytesAvailable()` in `SfxMixer` and report a generator-style
chunk (200 ms of the sink's format, rounded down to whole frames). The mixer
always has something to hand out — silence plus whatever voices are queued.

Alongside the fix, the audio path grew the health counters that made the
diagnosis possible in the first place:

| Counter | Meaning |
| --- | --- |
| `voicesStarted` | sounds queued by the game thread |
| `framesWritten` | frames that actually left the mixer (proves the sink pulls) |
| `peak` | loudest sample since the last status line (proves it is not silent) |

They are logged at startup (`pacman-audio: device … state …`), every 10 s while
the game is playing (`pacman-audio: playing triggers 50 sink 0 error 0 frames
645824 seconds 13.4547 peak 1`) and whenever the sink leaves `ActiveState`.
On-device logcat now proves the chain end to end, and `dumpsys
media.audio_flinger` shows our `AudioTrack` being added and removed with the
app.

Two smaller fixes came out of the same round:

- `kSfxGain` (0.6, the old `SoundEffect` volume) was declared but never applied
  to the mixed samples — it is now applied per voice.
- Two overlapping munches summed to 1.016 → clipping. The mix is now clamped to
  [-1, 1] after summing (peak reads exactly 1).

**Regression guard:** `ViewTest.testMixerFeedsTheAudioSink` asserts that the sink
reads frames and that the peak is > 0 (skipped when the machine has no audio
device).

**Device result after the fix:** 0 `pacman-perf: tick gap` lines, 0 `sfx
trigger blocked`, one benign 41 ms render-thread gap at startup, and the user
confirmed on the phone that the munches are **clearly audible** and that the
game plays **without lag**.

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
| Install on a device | ✅ Done | Tecno A059 (Android 16, arm64-v8a, fr-BE); `adb install -r` + launch OK, bundled levels confirmed in logcat |
| Device runtime testing | 🟢 In progress | levels + touch verified; **audio mixer verified** (silent-mixer bug found and fixed, §8); lifecycle/orientation/back button still open |

**Overall: build pipeline 100% complete; first device test done, device testing in progress.**

---

## Known runtime gaps before first launch

1. ~~**Levels directory is a build-host path.**~~ **Resolved**: `levels/*.txt` is now
   embedded as Qt resources (`pacman_levels` in `app/CMakeLists.txt`), and when the
   compiled-in `PACMAN_LEVELS_DIR` does not exist `GameController::materializeBundledLevels()`
   copies them into `QStandardPaths::AppDataLocation` for `LevelManager::loadDirectory`
   (core stays Qt-free). Covered by `ViewTest.testBundledLevelsAreShipped`.
2. ~~**Touch input**~~ **Resolved**: swipe/drag with a dead zone + tap-to-start in
   `Main.qml` (commit `1623f57`); confirmed working by hand on the phone.
3. **Audio**: per-play `QSoundEffect` restarts caused every frame gap (root cause
   §7 above); reworked into the persistent-stream mixer (commit `bedd1dc`).
   That mixer was in turn silent until `bytesAvailable()` was overridden (§8);
   both are fixed and verified on the phone — audio flows, no frame gaps.
4. **Lifecycle**: implemented (`GameController::setSuspended()` on
   `Qt::ApplicationSuspended`, tick timer stopped, frame clock restarted on
   resume); covered by `ViewTest.testSuspendingFreezesTheModel`, not yet
   re-verified on a device.
5. **Orientation / safe areas / back button**: implemented with a custom
   activity and manifest (§9); covered by two `ViewTest` cases, not yet
   re-verified on a device.
6. **Signing**: currently a shared debug keystore; release signing needs a real key.

---

## 9. The app ships its own activity and manifest

The orientation lock, the immersive mode and the back button need an
activity of our own — Qt's `QtActivity` offers none of them. Two pieces make
that work:

**The manifest** (`android/package/AndroidManifest.xml`) replaces the one
`androiddeployqt` generates. The package source directory is copied verbatim
into `build-android/app/android-build/`, so the custom manifest survives (it
is *not* regenerated). It has to carry everything Qt needs, because nothing is
merged with a generated one any more:

- `android:name="org.qtproject.qt.android.bindings.QtApplication"`,
- the `<meta-data android:name="android.app.lib_name" android:value="pacman_app"/>`,
- the `androidx.core.content.FileProvider` with `@xml/qtprovider_paths`.

**The activity** (`android/package/src/org/pacman/qt/MainActivity.java`)
extends `QtActivity` and adds:

- `setRequestedOrientation(SCREEN_ORIENTATION_SENSOR_LANDSCAPE)`,
- `FLAG_KEEP_SCREEN_ON` and the `SYSTEM_UI_FLAG_IMMERSIVE_STICKY` flags,
- the back-button interception.

**Why the back button needs JNI.** Android delivers the back press to the
activity, not to the Qt window, and Qt's own input delegate consumes it:
`QtActivityBase.onResume()` registers an `OnBackInvokedCallback` (API 33+)
and `QtInputDelegate` swallows the `KEYCODE_BACK` key event on older
platforms. So the activity takes the press over:

- `dispatchKeyEvent` intercepts `KEYCODE_BACK` (hardware button, older
  platforms) and consumes it, so the framework never closes the activity;
- on API 33+ the activity registers its own `OnBackInvokedCallback` — **after**
  `super.onResume()`, because Qt registers its callback there and the
  dispatcher keeps only one callback per priority.

Both paths call the native method `nativeBackPressed()`, which is
`Java_org_pacman_qt_MainActivity_nativeBackPressed` in `app/AndroidBridge.cpp`.
The symbol is exported by `libpacman_app_arm64-v8a.so` (the library the JVM
loads), queues the call onto the Qt thread and emits `backPressed()`, which
the QML side turns into the pause menu.

Verification of the packaged APK (no device needed):

```sh
# the manifest in the APK is ours
aapt2 dump xmltree --file AndroidManifest.xml pacman_app-signed.apk | grep screenOrientation
#   → http://schemas.android.com/apk/res/android:screenOrientation(0x0101001e)=6  (sensorLandscape)
aapt2 dump badging pacman_app-signed.apk | grep launchable-activity
#   → launchable-activity: name='org.pacman.qt.MainActivity' label='Pac-Man'

# the Java class was compiled and is in the dex
unzip -l pacman_app-signed.apk | grep classes.dex

# the JNI symbol is exported
nm -D build-android/app/android-build/libs/arm64-v8a/libpacman_app_arm64-v8a.so \
    | grep nativeBackPressed
#   → 0000000000091718 T Java_org_pacman_qt_MainActivity_nativeBackPressed
```

---

## Files involved

```
build-android.sh                    # full pipeline (configure → build → APK → sign)
build-android-core.sh               # core-only build (kept as a fast smoke test)
app/CMakeLists.txt                  # + QT_ANDROID_COMPILE_SDK_VERSION property
core/src/Level.cpp                  # classic-locale istringstream float parsing (portable)
app/SfxPlayer.{hpp,cpp}             # persistent-stream mixer: bytesAvailable() override,
                                    # gain, clamp, health counters (§8)
app/AppSettings.{hpp,cpp}           # persistent settings (sound, slow mode)
app/ScreenInsets.{hpp,cpp}          # safe-area insets (QWindow::safeAreaMargins)
app/AndroidBridge.{hpp,cpp}         # JNI back-button bridge (§9)
app/GameController.{hpp,cpp}        # suspend/resume, time scale, settings ownership
tests/test_view.cpp                 # + ViewTest.testMixerFeedsTheAudioSink (regression guard),
                                    #   testBackKeyOpensThePauseMenu,
                                    #   testAndroidBackBridgeOpensThePauseMenu,
                                    #   testSuspendingFreezesTheModel,
                                    #   testSettingsArePersisted, testSlowModeScalesTheModel
android/icon/                      # launcher-icon artwork + generate-icons.sh (inkscape)
android/package/                   # QT_ANDROID_PACKAGE_SOURCE_DIR: the manifest, the
                                   # activity (src/org/pacman/qt/MainActivity.java)
                                   # and the launcher icon res/
~/Qt/6.12.0/android_arm64_v8a/      # Qt kit (4 cmake dirs restored from armv7 + bitness fix)
```

---

**Status:** ✅ Build pipeline working end-to-end; installed and launched on hardware;
audio verified on the phone (mixer fixed + audible, no frame gaps); lifecycle,
orientation/safe areas and the back button implemented (own activity + manifest),
covered by headless tests and awaiting a device pass
**Next Action:** a phone pass over the new lifecycle/back-button behaviour, then
the Phase 6 items in `todo.md`.
