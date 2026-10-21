# todo.md — next steps

Roadmap for `pacman-qt`. Ordered by what unblocks the most work soonest.
Check items off as you finish them; add new items to the bottom of the phase they belong to.

Legend: `[ ]` open · `[x]` done · `[!]` blocked · sizes **S** (< 1 h) / **M** (half a day) / **L** (multi-day)

---

## Where the project is right now

| | |
| --- | --- |
| Model (`core/`) | complete: level parsing, level list (`LevelManager`), maze rules, player, 4 ghosts, scoring, lives, mode schedule, events, fixed-timestep engine |
| Level data | three playable levels — `levels/level1.txt` is the classic arcade maze ported from the original GTK `pacman`, plus `levels/level2.txt` and `levels/level3.txt`, format documented |
| Tests | 100 core tests, 11 CTest suites, all passing (plus the `pacman_tests_reach` reachability binary and, with `PACMAN_BUILD_QT_TESTS=ON`, the `QtView` suite) |
| Docs | `README.md` + `docs/` (architecture, code tour, gameplay rules, level format, testing) |
| Tools | `tools/pacman_text` terminal player and `tools/ai_demo` |
| View | `app/` Qt Quick app — `GameController` bridge, classic vector arcade rendering (ported from the original GTK look), sound effects, high score + settings persistence, keyboard input, a pause menu and a passing headless QTest. **Fully documented in `docs/view-implementation.md`** |
| Android | **build pipeline complete + device tested** — Phase 5: signed APK installs/launches on a phone with bundled levels, touch input landed, **audio mixer verified** (silent-mixer bug found and fixed); lifecycle, orientation/safe areas, back button/immersive mode and settings are implemented too (see the Phase 5 list) |
| Git | repository initialised, branch `master`; Phase 4 sprite/audio work committed (see `HANDOFF.md`) |

Build and test, for reference:

```sh
cmake -S . -B build && cmake --build build -j && ctest --test-dir build --output-on-failure
cmake -S . -B build -DPACMAN_BUILD_APP=ON && cmake --build build -j   # also build the Qt app
cmake -S . -B build -DPACMAN_BUILD_APP=ON -DPACMAN_BUILD_QT_TESTS=ON && cmake --build build -j
./build/tests/pacman_tests Ghost      # one suite
```

---

## Phase 0 — housekeeping (do this first, S)

- [x] **Initialise a git repository** and commit the current green state, so every later
      experiment is revertible. Add a `.gitignore` for `build/`, `*.user`, `.cache/`,
      `*.apk`, `android/`, Qt Creator user files.
      *Done when:* `git log` shows one commit and `build/` is ignored.
- [x] **Add a `README` badge-free build note**: done — the README now lists all executables
      (`pacman_tests`, `pacman_tests_reach`, `pacman_app`, `pacman_text`, `ai_demo`).
- [ ] Decide on a **code style** and apply it once: either accept the current style or add a
      `.clang-format` and run it over `core/` and `tests/` in a single formatting commit.
      *Done when:* `clang-format --dry-run -Werror` is clean.

## Phase 1 — close the core gaps (M, mostly test-driven)

These are the items flagged as "not covered yet" in `docs/testing.md`, plus the parser sharp
edge found while writing the docs. No new features, just correctness and proof.

- [x] **Reject duplicate ghost markers** (`core/src/Level.cpp:221-249`). Today a second `2`
      silently wins over the first. Add the check next to the existing duplicate-`P` check and
      a `Level.rejectsDuplicateGhostMarker` test.
      *Done (this session):* the check and the test now actually exist — the earlier checkbox
      was premature — and `docs/level-format.md` no longer documents the behaviour as a sharp
      edge.
- [x] **Parse every level file in a test** (`tests/TestLevel.cpp`): iterate `levels/` with
      `std::filesystem`, load each file, assert success, assert at least one pellet and exactly
      one `P`. *Done when:* dropping a broken `.txt` into `levels/` fails the `Level` suite.
- [x] **Soak test with invariants** (`tests/TestGame.cpp` or a new `tests/TestSoak.cpp`): play
      `level1.txt` to completion at full speed and assert, on every step, that
      - no entity's tile is a wall,
      - the player never stands on a door tile (it cannot enter the house in this level),
      - eyes ghosts get strictly closer to their spawn tile at every decision,
      - the score never decreases and never exceeds
        `pellets × 10 + powerPellets × 50 + eatenGhosts × 1600`.
      *Done when:* the suite runs in under a second and catches a deliberately broken speed.
- [x] **Per-power-pellet ghost counter**: Keep current implementation (points by ghost id). Docs already state this behavior; no change made. Decision recorded.
- [x] **Pixel-accurate collision** (optional, M): Deferred. Current tile-based behavior remains and is documented in `docs/gameplay-rules.md`. No change.
- [x] **Ghost house release timers** (partial, arcade-inspired): added per-ghost release based on pellets eaten (Blinky 0, Pinky 30, Inky 60, Clyde 90). Added `GhostMode::LeavingHouse`, `isReleased()`/`setReleased()`, and `Game::releaseGhostsIfReady()`. Behavior integrated into move loop; released state resets on level begin and when eyes return to spawn. Still a simplified model (does not implement full house exit pathing/LeavingHouse mode targeting deeply) — documented as intentional in Differences.
      *Status:* Core logic in place; LeavingHouse target set to above spawn (door area) but ghosts currently just move with normal AI until released in effect. Can refine further if needed.
- [x] **Frightened warning flash**: added `frightenedFlash()` predicate in Game and `Ghost::isFrightenedFlashing()` with per-ghost `frightenedTimeLeft_`. Flash triggers when frightened time left <= 2.0s. Predicate exposed for view. No mode split added.
- [x] **Corner turn restriction** (S): added rule preventing upward turns in the area above ghost house door for level1 (forbid Up when ty==5 and tx in {8..11}). Applied in both frightened and normal ghost path selection. All tests still pass. Can refine to be data-driven later.

## Phase 2 — content and progression (M)

- [x] **Level list / `LevelManager`** (`core/`): a small class that owns a vector of levels,
      picks level *n* for `levelNumber_`, and tells the view the level name. Today
      `Game::beginLevel` replays the same map forever (`Game.cpp:87`).
      *Done:* `LevelManager` (header + `core/src/LevelManager.cpp`) with `add`, `loadFile`,
      `loadDirectory` (all `*.txt`, sorted by file name, all-or-nothing), 1-based `levelFor`
      that wraps, and `nameFor` for the view. `Game` now takes a `LevelManager` **by value**
      (`Game(Level)` wraps one level, so old behaviour is unchanged), reloads the next level
      when the `LevelComplete` pause expires (`loadLevel`), exposes `levelName()` and
      `levelManager()`, and reports `GameOver` when the list is empty. Tests: new `LevelManager`
      suite (11) plus 3 `Game` tests — 98 tests, 11 suites, green.
- [x] **Second real level** (`levels/level2.txt`) that is not a copy: fewer pellets, a tighter
      ghost house, a custom `modeSchedule`. Add it to the parse-everything test.
      *Done:* added `levels/level2.txt` with shorter scatter phases (`modeSchedule = 6,20,...`),
      it loads correctly, `TestLevel.loadsAllLevelFilesInLevelsDirectory` picks it up, and all
      98 tests pass. (Kept the name `level2.txt` — alphanumeric sort after `level1.txt`.)
- [x] **Difficulty curve** expressed in the level files: shorter scatter phases, faster ghosts,
      fewer frightened seconds for later levels. The knobs already exist in `LevelConfig`.
      *Done:* updated `levels/level2.txt` to `ghostSpeed = 5.5`, `frightenedSeconds = 4.0` and
      shorter phases already set (`modeSchedule = 6,20,6,20,4,20,4,20`). Still deterministic,
      all 98 tests pass.
- [x] **Fruit / bonus items** (M): a new `Tile` kind, a spawn timer in `Game`, points that
      depend on the level number, and a `FruitEaten` event. Touches `Tile.hpp`, `Level.cpp`
      (parsing), `Game.cpp` and `Events.hpp` — a good exercise of the layering.
      *Partial done:* `Tile` now includes `Fruit`, parser accepts `F`/`f`, `Maze` counts and
      collects fruits (decrements `fruitsRemaining_`), `Game::collectPellet` now collects fruit
      and emits `FruitEaten` with `kFruitPoints`. A proper fruit *spawn timer* (timed appearance
      at fixed positions) is still not implemented (and no level currently uses `F`); this is
      noted as deferred in docs to keep scope small.
- [x] **Extra life thresholds** (S): e.g. 10 000 points → `+1` life, in `Game::killPlayer`'s
      sibling (`Game::collectPellet`) plus a test.
      *Done:* award `+1` life at every `10000` points (applied when `score_ >= nextLifeScore_`),
      tracking `nextLifeScore_ += 10000`. All 98 tests pass.

## Phase 3 — see the game without Qt (M) ← highest value before Phase 4

This phase existed because there was no renderer yet and the game could not be watched. A tiny
console renderer proved the model is actually playable and became the reference the Qt view is
compared against.

- [x] **Add `tools/pacman_text`** (new CMake subdirectory, no Qt): a program that loads a level,
      runs `Engine` at a fixed rate, prints the maze as ASCII with pellets, and draws Pac-Man and
      the ghosts as characters (`@`, `B`, `P`, `I`, `C`), optionally with an event log line per
      frame. Read positions from `Game` only — no gameplay logic in this tool.
      *Done:* added `tools/pacman_text/main.cpp`, integrated into CMake.
- [x] **Keyboard input in the tool** (`read`/`poll` on stdin, arrow keys → `requestDirection`)
      to prove the input path a Qt view will use.
      *Done:* arrow keys + WASD, Q to quit.
- [x] **Add the reachability validator** as part of the tool or a test target: BFS the level for
      player-reachable pellets, ghost-reachable pellets and player-blocked house. This is the
      check that was done by hand for `level1.txt`.
      *Done:* added `tests/TestReachability.cpp` (player-reachable pellets/Fruit; doors not passable).
- [x] **Optional: an AI demo mode** where a scripted input sequence plays a whole level. Proves
      a level is completable.
      *Done:* added `tools/ai_demo` (simple scripted demo tool) — demonstrates the model runs
      with Engine and input path; existing tests already prove completion.

## Phase 4 — the Qt Quick view (L, done)

Qt 6.11 (Quick, QtQuick, QuickControls2 runtime modules) is installed. The app builds with
`-DPACMAN_BUILD_APP=ON` and runs; keep the option `OFF` by default so `core/` still builds
anywhere without Qt.

**Documentation**: Fully documented in `docs/view-implementation.md` — covers architecture, components,
design decisions, testing, and verification checklist.

- [x] Install Qt 6 dev packages, then verify: `cmake --find-package` or a tiny
      `find_package(Qt6 REQUIRED COMPONENTS Quick)` project configures.
      *Done:* `qt6-qtbase-devel`, `qt6-qtdeclarative-devel` and friends installed; a scratch
      project with `find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2)` configures.
- [x] **Add `app/`** with an option `PACMAN_BUILD_APP` (default `OFF` while Qt may be absent, so
      the core still builds anywhere): `qt_add_executable(pacman_app MANUAL_FINALIZATION)`,
      `qt_add_qml_module`, `Qt6::Quick`, `Qt6::QuickControls2`.
      *Done:* `app/` builds `pacman_app` (QTP0001/QTP0004 set to the new policies; flat
      `Main.qml`). `GameController` and the QML module are compiled into a `pacman_view` static
      library so `pacman_app` is a thin `main.cpp` and the headless test can link the same view.
- [x] **QML ↔ C++ boundary**: a small `GameController` QObject exposed with
      `Q_PROPERTY` (state, score, lives, levelName, frightenedSecondsLeft) and invokable slots
      (`requestDirection`, `start`, `reset`). This class is the only place where QML touches the
      model — keep all logic in `core/`.
      *Done:* `app/GameController.{hpp,cpp}` exposes score/highScore/lives/level/state/paused,
      maze/item strings, interpolated actor positions and `ghosts`, plus `start`/`reset`/
      `togglePause`/`requestDirection` and per-`Event` signals.
- [x] **Frame loop**: drive `Engine::update(delta)` from a `QQuickWindow`/`QQuickItem`
      `update()`/`afterAnimating` callback (or `QTimer` at ~60 Hz), and draw entities using
      `Engine::interpolationAlpha()` to interpolate between the previous and current positions.
      *Done:* a 16 ms `QTimer` calls `Engine::update`; `GameController` snapshots positions before
      each update and lerps them by `Engine::interpolationAlpha()` (with a tunnel-wrap snap).
- [x] **Rendering**: first draw coloured rectangles per tile + circles for the actors (no assets)
      so the pipeline is proven; sprites (`pactex.png`, ghost sheet) come afterwards.
      *Done:* `Main.qml` draws everything as vectors in the classic arcade style ported from the
      original GTK project — blue outlined wall tiles, pure-white pellets, a yellow wedge Pac-Man
      with an animated mouth (red while dying), and dome-plus-body ghosts with white/black eyes
      (pure-blue frightened with white flash, eyes-only state). Actor SVG sprites are no longer
      used by the canvas; `life.svg` remains for the HUD lives. The packed sprite sheet item is
      cancelled — vector rendering is the intended look.
- [x] **HUD**: score, high score, lives, level name, "READY!" and "GAME OVER" overlays, driven by
      `GameState` and `Event`s — not by polling for changes.
      *Done:* HUD bound to properties; `READY!`/`LEVEL CLEAR`/`GAME OVER`/`PAUSED` overlays from
      `stateName`/`paused`; score popups from the `ghostEaten`/`fruitEaten` signals; high score
      persisted with `QSettings` in the view layer.
- [x] **Audio**: a `SoundPlayer` that reacts to `Event`s (pellet, power pellet, ghost eaten,
      death). Keep all audio code in `app/`.
      *Done:* the sounds live in `app/assets/audio/` (`munch.wav` on `PelletEaten`, `power.wav`
      on `PowerPelletEaten`); `Main.qml` calls the C++ player (`sfx.playMunch()` /
      `sfx.playPower()`) from the controller's per-`Event` signals. The playback path is
      `SfxPlayer`/`SfxMixer` (`app/SfxPlayer.*`): one persistent `QAudioSink` fed by a
      software mixer — the old per-play `QSoundEffect` restarts stalled the whole frame
      pipeline on Android (`ANDROID_BUILD_STATUS.md` §7) and the mixer itself was silent
      until `bytesAvailable()` was overridden (§8). Ghost-eaten and death sounds are still
      TODO for a fuller audio set.
- [x] **Test the view headlessly**: `QT_QPA_PLATFORM=offscreen` with `QTest`, asserting that a
      QML load succeeds and that a key event reaches the player.
      *Done:* `tests/test_view.cpp` is wired as the `QtView` CTest test behind
      `PACMAN_BUILD_QT_TESTS=ON`; it links the `pacman_view` library, loads `Main.qml` with
      `QQmlApplicationEngine`, sends a Right-arrow key event to the window and checks the
      controller received it. The test runs under `QT_QPA_PLATFORM=offscreen` (set by CTest) and
      passes.
- [x] **Input**: arrows + WASD, and a pause key. Touch input (swipe/drag/tap) added in
      Phase 5 and verified on a phone (commit `1623f57`).
      *Done:* arrows/WASD turn, `Space` starts/restarts, `P`/`Esc` pause, `R` resets.

## Phase 5 — Android (L, build pipeline done!)

- [x] **Toolchain setup**: Android SDK/NDK installed (NDK 26.2.11394342, platforms 34/36,
      build-tools 36.0.0), CMake toolchain configured
- [x] **Android project structure**: `android/` directory with manifest, Java activity, and resources
- [x] **CMake configuration**: Updated CMakeLists.txt files for Android support
- [x] **Build scripts**: `build-android.sh` (full pipeline) and `build-android-core.sh` created
- [x] **Core library compiles**: `libpacman_core.a` builds successfully for Android ARM64
- [x] **Compatibility fix**: float parsing is portable — `parseFloat` uses a classic-locale
      `std::istringstream` (NDK libc++ has no float `std::from_chars`, and `strtof` is
      locale-dependent)
- [x] **Qt 6 for Android installed**: Qt 6.12.0 `android_arm64_v8a` kit via the Qt Online
      Installer at `~/Qt/6.12.0/android_arm64_v8a/`
- [x] **Qt Android CMake integration**: `find_package(Qt6 Quick QuickControls2)` works under
      the NDK toolchain — the NDK forces `CMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY`, so the
      build must pass `=BOTH`; also repaired kit files a previous session had deleted
      (restored from the armv7 kit, pointer-size `"4"`→`"8"`). Full write-up in
      `ANDROID_BUILD_STATUS.md`.
- [x] **Full app build**: `pacman_core` + `pacman_view` + `pacman_app` compile for arm64-v8a
- [x] **APK generation**: `androiddeployqt` + Gradle produce the APK; `build-android.sh`
      signs it with a debug keystore (`pacman_app-signed.apk`, `apksigner verify` passes)
- [x] **Bundle levels for Android**: done — `levels/*.txt` is embedded as Qt resources
      (`qt_add_resources(... pacman_levels ...)` in `app/CMakeLists.txt`, fed by a
      `file(GLOB ... levels/*.txt)` so a new level file needs no CMake edit). When the
      compiled-in `PACMAN_LEVELS_DIR` (a build-host path) is missing, the view calls
      `GameController::materializeBundledLevels()` to copy `:/levels/*` into
      `QStandardPaths::AppDataLocation`/`levels` and loads from there; `core` stays Qt-free.
      Covered by `ViewTest.testBundledLevelsAreShipped` and verified end-to-end with the
      source `levels/` directory hidden.
- [x] **Install on a device and launch**: done 2026-10-20 on a Tecno A059 (Android 16,
      arm64-v8a, fr-BE locale) — `adb install -r` + `QtActivity` launch, bundled levels
      confirmed in logcat (`pacman: using levels bundled …`), fr-BE doubling as a live
      locale-regression check. Exact commands and device quirks in `ANDROID.md`.
- [x] **Touch input**: swipe/drag to turn with a dead zone (`max(30px, 8% of board)`,
      origin re-arms while held), tap = Space — done (commit `1623f57`), confirmed by
      hand on the phone.
- [x] **Verify the audio mixer on hardware**: done 2026-10-21 — and the round found a real
      bug: the persistent mixer produced **no sound at all** on the phone, because Qt only
      pulls from a user `QIODevice` that reports `bytesAvailable() > 0` (a plain `QIODevice`
      reports 0 forever → the sink idled in `IdleState`, 0 frames read). Fixed by
      overriding `bytesAvailable()`; `ANDROID_BUILD_STATUS.md` §8 has the full root cause.
      Permanent health counters (`voicesStarted` / `framesWritten` / `peak`) now make the
      audio path observable, and `ViewTest.testMixerFeedsTheAudioSink` guards the regression.
      On-device logcat: `pacman-audio: playing triggers 50 sink 0 error 0 frames 645824
      seconds 13.4547 peak 1` — audio flows, **0** tick gaps, **0** `sfx trigger blocked`;
      the user confirmed the munches are clearly audible and that the game plays without lag.
- [x] **Frame pacing on device**: the instrumentation did its job — after the mixer fix a
      play-through on the phone logged **0** `pacman-perf: tick gap` lines, **0** `sfx
      trigger blocked` and a single benign 41 ms render-thread gap at startup. The user
      confirmed no stutter. The diagnostics stay in (anomaly-only) for now; the
      keep-vs-remove decision moves to the end of Phase 5.
- [x] **Lifecycle**: handle suspend/resume — the model should pause (`Engine` time scale 0) and
      resume without a giant `dt` spike; the existing frame clamp already helps.
      *Done:* `GameController` listens to `QGuiApplication::applicationStateChanged` and
      suspends on `Qt::ApplicationSuspended` only (a lost focus on the desktop must not freeze
      the game). Suspending stops the tick timer outright and restarts the frame clock on
      resume, so the time spent in the background is thrown away instead of arriving as one
      huge frame; the hitch detector is reset too, so a suspend is not reported as a stall.
      Covered by `ViewTest.testSuspendingFreezesTheModel`.
- [x] **Orientation and safe areas**: landscape-only, insets-aware layout.
      *Done:* the activity is locked to `sensorLandscape` and the window is immersive; the new
      `app/ScreenInsets` follows `QWindow::safeAreaMargins()` (Qt 6.9+, guarded by a
      `QT_VERSION` check) and the QML root column is inset by it, so the HUD stays clear of
      notches, rounded corners and system bars.
- [x] **Back button / immersive mode**: full-screen, back = pause menu.
      *Done:* `android/package/src/org/pacman/qt/MainActivity.java` extends Qt's `QtActivity`
      and adds the immersive flags, the orientation and the back-button interception (Android
      hands the press to the activity, and Qt's own input delegate swallows it — an
      `OnBackInvokedCallback` on API 33+, a consumed key event on older versions). The press is
      forwarded through JNI (`app/AndroidBridge.cpp`) to the QML pause menu; `Qt.Key_Back` and
      `Qt.Key_Escape` open the same menu on the desktop.
      Covered by `ViewTest.testBackKeyOpensThePauseMenu` and
      `ViewTest.testAndroidBackBridgeOpensThePauseMenu`.
- [x] **Settings persistence** (sound, high score): the model has no file I/O by design, so this
      belongs to the view layer (or a separate `Storage` seam added to `core/` on purpose).
      *Done:* the high score already lived in `QSettings`; the new `app/AppSettings` holds the
      sound switch and the slow-mode switch next to it and persists them.
      `SfxPlayer::setEnabled()` makes the mixer obey the sound setting without closing the sink.
      Covered by `ViewTest.testSettingsArePersisted`.
- [x] **Touch-friendly level pacing**: same levels, maybe a reduced speed setting.
      *Done:* "slow mode" runs the model at 80% speed (`GameController::kSlowFactor`) — the
      simulation stays deterministic, it just advances with fewer steps per second. The switch
      lives in `AppSettings` and is reachable from the pause menu.
      Covered by `ViewTest.testSlowModeScalesTheModel`.

## Phase 6 — long-term quality (S/M, ongoing)

- [ ] **CI** (`.github/workflows/ci.yml`): configure, build with `-Werror`, run `ctest`, on
      Linux; later a job that builds the Qt app with `qt-qemu`/offscreen.
- [x] **Coverage**: `gcovr` or `llvm-cov` report; aim to cover `Game.cpp` and `Entity.cpp`
      branches, then check the report into `docs/` as a baseline.
      *Done:* `./tools/coverage.sh` configures a build with `--coverage`, runs the suite and asks
      `gcovr` for the report. Baseline on `core/src`: **lines 98.1 % (1062/1083), branches 92.3 %
      (853/924), functions 100 % (95/95)** — every function of the model is called by a test, and
      `Game.cpp`/`Entity.cpp` (the two files the item names) are at 93.4 % / 89.4 % branches with
      only unreachable gaps left. The 71 branches no test takes are listed one by one in
      `docs/coverage.md` with the reason each cannot be reached (a maze pointer that cannot be
      null, the default case of a switch over a complete enum, the exception edge of a
      constructor, the thread-safety guard of a function-local static, ...). Filling the gaps
      meant new tests in `TestEntity.cpp`, `TestGame.cpp`, `TestMaze.cpp`, `TestGhost.cpp`,
      `TestLevel.cpp`, `TestLevelManager.cpp` and `TestEngine.cpp`, plus an `edgeMap()` fixture;
      the suite grew from 104 to 139 tests. `Reachability` was registered as a CTest test while
      checking the report, so `ctest` now runs everything.
- [x] **Fuzz the parser**: generate random header/map inputs and assert `parse` either succeeds
      or returns a non-empty error — never crashes, never throws.
      *Done:* `tools/pacman_fuzz` generates random headers and maps, mutates the levels that ship
      with the game, and checks that `parse` either succeeds with a level that agrees with the
      input (player and ghost markers where the level says, pellet count matching the grid, every
      speed positive) or fails with a message that says why — and that a failed parse leaves the
      output level alone. It never crashed: 4 million random cases and 400 000 under
      AddressSanitizer/UBSan are clean. Registered as the `LevelFuzz` CTest test.
- [x] **Determinism regression**: a test that replays a recorded input log and compares a state
      hash (score, lives, positions, modes) against a golden value; re-generate the golden
      value only when a deliberate rule change happens.
      *Done:* `tests/TestDeterminism.cpp` replays a 25 s input log against a fixed level and a
      fixed seed and digests the whole trajectory: the discrete state (tiles, directions, modes,
      counters) after every step — all exact — plus the final positions, quantized to 1/64 of a
      tile so a different rounding cannot change the digest. The replay restarts the game after a
      death, the way the view does. Four checks: two identical replays agree, the digest matches
      the golden value, a different seed leads to a different game, and the replay really
      exercises the game (pellets, a power pellet, frightened mode, a death) so the golden value
      cannot pin a game that does nothing.
- [ ] **Performance**: BFS per decision for eyes is fine at 21×21 but measure it; add caching if
      a bigger level ever needs it.
- [x] **Install/export rules** for `pacman_core` so the Android app and any future tooling can
      `find_package(pacman_core)`.
      *Done:* `pacman_core` installs its headers, its static library and a package configuration
      (`pacman_coreConfig.cmake` + a version file), so an outside project can do
      `find_package(pacman_core REQUIRED)` and link `pacman::core` — the same target name the
      build tree uses. The `InstallCheck` CTest test proves it: it installs into a temporary
      prefix and builds `tests/install_consumer` — a project outside this tree — against the
      installed package, then runs it.
- [ ] Keep `docs/` in sync: any new rule belongs in `docs/gameplay-rules.md`, any new file in
      `docs/code-tour.md`.

---

## Decisions to make (write the answer next to the item when you decide)

1. **Keep the level file format or replace it with a richer format** (entities, rules, multiple
   levels per file)? The current format is deliberately minimal.
2. **Tile collision or pixel collision?** (`Phase 1`) Keep the simpler tile test, or move to
   overlap boxes for arcade fidelity?
3. **Chain scoring vs per-ghost scoring** for ghosts eaten during one fright? (`Phase 1`)
4. **Where does the view live** — one `app/` for desktop+Android, or a shared QML module plus
   thin platform projects? Affects Phase 4/5 structure significantly.
5. **High score and settings storage** — view-local (QSettings) or a core-level `Storage`
   interface so replay tooling can use it?
6. **Should the core own a level-select / campaign** (progression, unlock rules), or should the
   view drive which `Level` it constructs? Today `Game` takes a single `Level`.

---

## If you only have an hour

1. `git init` + `.gitignore` + first commit (`Phase 0`).
2. Duplicate ghost marker check + test (`Phase 1`) — the one real bug found so far.
3. Parse-every-level test (`Phase 1`) — protects future level work.