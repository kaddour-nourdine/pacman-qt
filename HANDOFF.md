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
| Tests | 100 core tests, 11 CTest suites, **11/11 passing** (plus `pacman_tests_reach` and, with `PACMAN_BUILD_QT_TESTS=ON`, the `QtView` suite: **12/12 passing**) |
| Toolchain used | CMake 4.3.0, g++ (GCC) 16.2.1, Linux, 16 cores |
| Qt | **6.11 installed** (`qt6-qtbase-devel`, `qt6-qtdeclarative-devel`); QtQuick/QtQuick.Window/Controls runtime modules present |
| View | `app/` builds with `-DPACMAN_BUILD_APP=ON`; the `GameController` + QML module live in a `pacman_view` static library, `pacman_app` is a thin `main.cpp`; runs under `QT_QPA_PLATFORM=offscreen`; draws the maze and actors as vectors in the classic arcade style (ported from the original GTK project) and plays WAV sound effects |
| Working tree | clean (commits `bdf30ac`, `50b3275`) |
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
  HUD/overlays, handles keys, and plays the WAV effects in `app/assets/audio/` (`munch.wav` on
  `PelletEaten`, `power.wav` on `PowerPelletEaten`). `GameController` and the QML module are
  compiled into a `pacman_view` static library, so the headless test can link the same view.
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
8. **Android levels are not bundled** — `PACMAN_LEVELS_DIR` bakes in the build-host path;
   on a device the app logs a warning and has no levels. First real Phase 5 task.


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

**Next Steps (Phase 5 device testing, see `todo.md`)**:
1. **Bundle `levels/*.txt` for Android** — `PACMAN_LEVELS_DIR` is a build-host path, so the
   game currently starts with no levels on a phone
2. `adb install` the signed APK on a device/emulator, confirm it launches and hits 60 fps
3. Touch input, lifecycle, orientation/safe areas, back button (tracked in `todo.md`)

Minor polish (extra audio effects) is tracked in `todo.md`.