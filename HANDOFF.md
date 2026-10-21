# HANDOFF — session notes

Written at the end of the first working session so the next one can start without
re-deriving anything. `todo.md` holds the plan; `docs/` holds the explanations. This file
holds only the state and the constraints.

## Verified state

| | |
| --- | --- |
| Working directory | `/home/nourdine/Development/pacman-qt` |
| Version control | git (branch master) |
| Build | clean, no warnings with `-Wall -Wextra -Wpedantic` |
| Tests | 100 core tests, 11 CTest suites, **11/11 passing** (plus `pacman_tests_reach` and, with `PACMAN_BUILD_QT_TESTS=ON`, the `QtView` suite: **12/12 passing**, 11 QTest cases) |
| Toolchain used | CMake 4.3.0, g++ (GCC) 16.2.1, Linux, 16 cores |
| Qt | **6.11 installed** (`qt6-qtbase-devel`, `qt6-qtdeclarative-devel`); QtQuick/QtQuick.Window/Controls runtime modules present |
| View | `app/` builds with `-DPACMAN_BUILD_APP=ON`; the `GameController` + QML module live in a `pacman_view` static library, `pacman_app` is a thin `main.cpp`; runs under `QT_QPA_PLATFORM=offscreen`; draws the maze and actors as vectors in the classic arcade style (ported from the original GTK project) and plays WAV sound effects |
| Working tree | clean at `e1d4d16` before the Phase 5 completion work (lifecycle, safe areas, back button, settings) |
| Device tested | Tecno A059 (AsteroidsEEA), Android 16, arm64-v8a, fr-BE — APK installed/launched, bundled levels + touch confirmed; **audio mixer verified** (silent-mixer bug found and fixed, see `ANDROID_BUILD_STATUS.md` §8). The lifecycle/back-button work is **not** on-device verified yet. |
| Last verified commands | `cmake -S . -B build -DPACMAN_BUILD_APP=ON -DPACMAN_BUILD_QT_TESTS=ON && cmake --build build -j16 && ctest --test-dir build` |

`build/` exists and is current, so a rebuild is incremental.

## Constraints agreed in this session

- Work **only inside this directory**. Do not probe or modify anything outside it, and do not
  install system packages without asking.
- `core/` must stay free of Qt, graphics, platform APIs, wall-clock time and file I/O other
  than reading level files (`Level::loadFile`, `LevelManager::loadDirectory`). This is what
  makes the model reusable on Android.
- Game rules live in `Game`/`Maze`/`Ghost`; the view may only read state, send input requests
  and react to `Event`s.
- The model is deterministic: fixed 1/120 s steps, seeded xorshift RNG, no `rand()`.

## What exists

- `core/` — the whole simulation (see `docs/code-tour.md` for the file-by-file tour).
- `levels/level1.txt`, `levels/level2.txt`, `levels/level3.txt` — three playable levels:
  `level1.txt` is the classic arcade maze ported from the original GTK `pacman` project;
  `level3.txt` has a shorter scatter schedule, faster ghosts and fewer frightened seconds.
- `core/LevelManager` — owns the campaign: `add`/`loadFile`/`loadDirectory`, 1-based
  `levelFor` (wraps), `nameFor` for the view. `Game` takes it by value, advances on
  `LevelComplete`, reports `GameOver` on an empty list.
- `tools/pacman_text` — ASCII terminal renderer with arrow/WASD input; `tools/ai_demo` — a
  scripted demo. Both drive `Engine` and only read `Game`.
- `app/` — the Qt Quick view (`PACMAN_BUILD_APP`, default `OFF`): `GameController` is the only
  bridge to the model (Q_PROPERTY + invokable slots + per-`Event` signals); `Main.qml` draws the
  maze/actors on a `Canvas` as vectors in the classic arcade style (ported from the original GTK
  project — the SVG actor sprites in `app/assets/sprites/` are no longer used at runtime; only
  `life.svg` remains for the HUD), shows the
  HUD/overlays, handles keys, and plays the WAV effects in `app/assets/audio/`
  (`munch.wav` on `PelletEaten`, `power.wav` on `PowerPelletEaten`). The playback itself is
  C++: `SfxPlayer`/`SfxMixer` (`app/SfxPlayer.*`) own one persistent `QAudioSink` and mix the
  decoded WAVs into it — QML only calls `sfx.playMunch()` / `sfx.playPower()`.
  `GameController` and the QML module are
  compiled into a `pacman_view` static library, so the headless test can link the same view.
- `app/AppSettings.{hpp,cpp}` — persistent settings (`QSettings`): sound on/off and slow mode
  (the model runs at 80% speed). Owned by `GameController`, reachable from QML as
  `game.settings`.
- `app/ScreenInsets.{hpp,cpp}` — follows `QWindow::safeAreaMargins()` (Qt 6.9+) and publishes it
  as the `insets` context property for the QML layout.
- `app/AndroidBridge.{hpp,cpp}` — the JNI back-button bridge
  (`Java_org_pacman_qt_MainActivity_nativeBackPressed`), exported by the app library on Android.
- `android/package/` — the `QT_ANDROID_PACKAGE_SOURCE_DIR`: the manifest that replaces the
  generated one, our activity (`src/org/pacman/qt/MainActivity.java`: landscape, immersive mode,
  back-button interception) and the launcher icon `res/`.
- `tests/` — custom harness, no external dependency; `tests/TestLevelManager.cpp` covers the
  loader (temp directories, all-or-nothing, sorted directory scan) and `tests/TestReachability.cpp`
  is a separate binary that BFS-checks every level. `tests/test_view.cpp` is a headless `QTest`
  (only built with `PACMAN_BUILD_QT_TESTS=ON`) that links `pacman_view`, loads `Main.qml` and
  checks a key event reaches the controller; it passes under `QT_QPA_PLATFORM=offscreen`.
- `README.md`, `docs/{architecture,code-tour,gameplay-rules,level-format,testing}.md`, `todo.md`.

## Open items carried over

1. **Qt app is unverified on a real display** — only checked headless (`offscreen`). Confirm the
   window, keyboard focus and `Canvas` rendering on a desktop session.
2. **Locale-sensitive parsing is fixed** — `Level.cpp` parses floats with a classic-locale
   `std::istringstream`, which is immune to both problems: `strtof` broke under Qt because
   `QGuiApplication` installs `fr_FR` with its comma decimal separator, and `std::from_chars`
   has no floating-point overload in NDK libc++. A regression test
   (`Level.parsesIndependentlyOfNumericLocale`) covers it.
3. `Game::checkCollisions` uses tile comparison, not sprite overlap — currently intentional and
   documented (`docs/gameplay-rules.md` §7), revisit only as a deliberate fidelity decision.
4. Ghost house release timers (pellet-based), frightened flash predicates and the corner-turn
   restriction are partially/explicitly implemented; see `docs/gameplay-rules.md` "Differences".
5. Known quirk: `Ghost::forbidUpwardTurn()` forbids Up entirely at the spawn tile row, so Blinky
   may bounce back into the house in `level1.txt` — verified by inspection, not yet fixed.
6. Phase 4: completed (build refactored to `pacman_view` library, headless `QtView` test passing,
   classic vector arcade rendering and sound effects added; see commits `6e1253b`, `e1e8a4a`).
   Minor polish remains documented in `todo.md` (extra audio effects; the packed sprite sheet
   item was cancelled — vector rendering is the intended look).
7. **Qt kit is hand-repaired**: `~/Qt/6.12.0/android_arm64_v8a/lib/cmake/{Qt6Core,Qt6Gui,
   Qt6Quick,Qt6QuickControls2}` were deleted by an earlier session and restored from the
   armv7 kit. If Qt ever misbehaves oddly, re-run a `find_package` probe first; the
   MaintenanceTool at `~/Qt/MaintenanceTool` is the official repair route (no local package
   cache though — needs network).
8. **Android levels are bundled** (resolved) — `levels/*.txt` ships inside the binary as Qt
   resources; when the build-host `PACMAN_LEVELS_DIR` is absent, the view copies them into
   `QStandardPaths::AppDataLocation` and loads from there
   (`GameController::materializeBundledLevels()`, tested by `ViewTest.testBundledLevelsAreShipped`).
9. **Audio mixer verified on hardware** (closed) — commit `bedd1dc` replaced per-play
   `QSoundEffect` restarts with a single persistent `QAudioSink` + software mixer
   (`app/SfxPlayer.*`) after on-device instrumentation proved the restarts caused
   every frame gap (~50 tick gaps/game → ~5 with sound muted; full chain in
   `ANDROID_BUILD_STATUS.md` §7). The first confirmation round then found the
   mixer was **completely silent**: Qt only pulls from a user `QIODevice` when it
   reports `bytesAvailable() > 0`, so the sink idled without reading a frame
   (`ANDROID_BUILD_STATUS.md` §8). Fixed by overriding `bytesAvailable()`, and
   guarded by `ViewTest.testMixerFeedsTheAudioSink` plus permanent health counters
   (`voicesStarted` / `framesWritten` / `peak`). On the phone: audio flows, 0 tick
   gaps, 0 `sfx trigger blocked`, and the user confirmed the game plays without lag.
   The perf diagnostics ship in the app and only log on anomalies — decide
   keep-vs-remove at the end of Phase 5.
10. **Phase 5 feature work is implemented, awaiting a device pass** — the four remaining
    `todo.md` items were closed in one session:
    - *Lifecycle*: `GameController` suspends on `Qt::ApplicationSuspended` (not
      `ApplicationInactive` — a desktop focus loss must not freeze the game): the tick timer
      stops, the time scale goes to 0 and the frame clock is restarted on resume, so the time
      spent in the background is thrown away instead of arriving as one huge frame.
    - *Orientation / safe areas*: the activity locks the orientation to `sensorLandscape` and
      goes immersive; `app/ScreenInsets` follows `QWindow::safeAreaMargins()` and the QML root
      column is inset by it.
    - *Back button / immersive mode*: Android hands the back press to the activity and Qt's own
      input delegate swallows it, so `android/package/src/org/pacman/qt/MainActivity.java`
      intercepts it (`dispatchKeyEvent` + an `OnBackInvokedCallback` registered **after**
      `super.onResume()`) and forwards it through JNI to the QML pause menu.
    - *Settings / pacing*: `app/AppSettings` persists the sound switch and the slow mode
      (the model runs at 80% speed — the simulation stays deterministic).

    All of it is covered by the headless `QtView` suite (11 cases) and the packaged APK was
    verified with `aapt2`/`nm` (see `ANDROID_BUILD_STATUS.md` §9) — but **no phone was
    connected**, so the on-device behaviour is still unproven.

## Suggested first action next session

```sh
cd /home/nourdine/Development/pacman-qt
git status && git log --oneline -8
cmake -S . -B build -DPACMAN_BUILD_APP=ON && cmake --build build -j16 && ctest --test-dir build
QT_QPA_PLATFORM=offscreen ./build/app/pacman_app    # quick smoke run
```

Phases 1–3 are complete and Phase 4 is **fully documented**. The view is refactored into a
`pacman_view` static library, the headless `QtView` test passes (12/12 CTest), and sprites/audio/
high-score persistence are in place. Phase 4 documentation is now complete in `docs/view-implementation.md`.

**Phase 5 (Android) — build pipeline complete, APK produced (2026-10-20)**:
- Android toolchain configured using the NDK's built-in CMake toolchain
- Qt 6.12.0 `android_arm64_v8a` kit installed via the Qt Online Installer at
  `~/Qt/6.12.0/android_arm64_v8a/`
- **`find_package(Qt6 …)` works under the NDK toolchain** — the NDK forces
  `CMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY` (re-rooting every search into the sysroot), so
  all Android CMake invocations must pass `-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH`
- Qt kit damage from an earlier session repaired: `lib/cmake/{Qt6Core,Qt6Gui,Qt6Quick,
  Qt6QuickControls2}` restored from the intact `android_armv7` kit
  (`armeabi-v7a`→`arm64-v8a` only in `*Targets*.cmake`), and the copied
  `*ConfigVersionImpl.cmake` pointer size fixed `"4"`→`"8"` (12 files)
- **Full app builds**: core + view + app compile for arm64-v8a, `androiddeployqt` + Gradle
  package it, and `build-android.sh` signs the result with a debug keystore
  (`build-android/app/android-build/build/outputs/apk/release/pacman_app-signed.apk`,
  ~68 MB, `apksigner verify` passes)
- Gradle needs a full JDK: script sets `JAVA_HOME=/usr/lib/jvm/java-27` (default
  `java-25-openjdk` is JRE-only)
- compileSdk raised to 36 via `QT_ANDROID_COMPILE_SDK_VERSION` (Qt's template bundles
  `androidx.core:core`, needs ≥ 36) while the NDK stays at API 34; SDK has
  `platforms;android-36` + `build-tools;36.0.0`, licenses accepted
- Portable float parsing in `core/src/Level.cpp`: classic-locale `std::istringstream`
  (NDK libc++ has no floating-point `std::from_chars`, and `strtof` is locale-dependent —
  it re-broke `Level.parsesIndependentlyOfNumericLocale`)
- Documentation: `ANDROID.md` (working guide), `ANDROID_BUILD_STATUS.md` (history/root causes)

**Device test session 2026-10-20 (afternoon) — what happened**:
- Installed and launched on a physical phone (Tecno A059, Android 16, arm64-v8a, fr-BE):
  bundled levels confirmed in logcat, first on-screen rendering OK
- Touch input implemented and confirmed working by hand (swipe/drag to turn, tap to
  start; commit `1623f57`)
- Pellet sounds caused visible stutter; root-caused with on-device instrumentation
  (tick/render probes) + a mute A/B build: per-play `QSoundEffect` restarts stalled the
  whole frame pipeline (~50 tick gaps/game → ~5 muted). Replaced by the persistent
  `QAudioSink` + software mixer `SfxPlayer`/`SfxMixer` (commit `bedd1dc`)

**Device test session 2026-10-21 — the mixer was silent, now it is not**:
- The confirmation round of §7's rework exposed a second, quieter bug: the game made
  **no sound at all** on the phone (sink in `IdleState`, 0 frames read) — and the
  desktop looked perfectly healthy, because a silent mixer is not an error.
- Root cause in Qt 6.12's own source
  (`qaudio_qiodevice_support_p.h`, `pullFromQIODeviceToRingbuffer`): Qt only pulls from a
  user `QIODevice` when `device->bytesAvailable() > 0`; a plain `QIODevice` reports 0
  forever. `SfxMixer` now overrides `bytesAvailable()` and claims a 200 ms chunk
  (generator style) → the sink finally reads.
- Permanent health counters were added so this can never be invisible again:
  `voicesStarted` / `framesWritten` / `peak`, logged at startup, every 10 s while
  playing and on sink-state changes (`pacman-audio: playing triggers 50 sink 0 error 0
  frames 645824 seconds 13.4547 peak 1`).
- Also fixed: `kSfxGain` (0.6) was never applied, and overlapping munches summed to
  1.016 → the mix is now clamped to [-1, 1] (peak reads exactly 1).
- Regression guard: `ViewTest.testMixerFeedsTheAudioSink` (skips without an audio device).
- Result on the phone: audio flows, **0** `tick gap`, **0** `sfx trigger blocked`, one
  benign 41 ms render-thread gap at startup; the user confirmed the munches are clearly
  audible and that the game plays without lag.

**First actions next session**:
```sh
cd /home/nourdine/Development/pacman-qt
git status && git log --oneline -8          # expect a clean tree
export PATH=$PATH:$HOME/Android/platform-tools
adb devices                                  # USB dropped twice — replug if empty
adb shell input keyevent KEYCODE_WAKEUP      # Qt main() does not advance while dozing!
adb shell wm dismiss-keyguard
./build-android.sh && adb install -r build-android/app/android-build/build/outputs/apk/release/pacman_app-signed.apk
adb logcat -G4M; adb logcat -c
adb logcat -v time > /tmp/logcat-device.txt &   # capture survives buffer rotation
adb shell am start -n org.qtproject.example.pacman_app/org.pacman.qt.MainActivity
```
Then verify on the phone what the headless tests can only approximate: the back button
opens the pause menu (and does not close the app), the game is landscape and full-screen,
backgrounding the app freezes the game and coming back does not replay the lost time,
and the sound/speed switches survive a restart.

Device quirks learned: the installed package is `org.qtproject.example.pacman_app`
(Qt's template — only `android/package/` is wired into the build, as the
`QT_ANDROID_PACKAGE_SOURCE_DIR` that carries the manifest, the activity and the launcher
icon); the launcher activity is now `org.pacman.qt.MainActivity` (ours, not Qt's);
our `qInfo` lands under
logcat tag `default` (Qt's own lines use `Qt`);
launching while the screen dozes and `main()` stalls until the screen wakes; on the
desktop Qt logs go to syslog unless `QT_FORCE_STDERR_LOGGING=1` is set.

**Still open**: a device pass over the new lifecycle/back-button work, release signing
(a real key instead of the shared debug keystore) and the decision whether the perf
diagnostics stay enabled (they only log on anomalies). The `todo.md` Phase 6 items
(CI, coverage, parser fuzzer, determinism regression) are untouched.

Minor polish (extra audio effects) is tracked in `todo.md`.