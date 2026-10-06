# todo.md — next steps

Roadmap for `pacman-qt`. Ordered by what unblocks the most work soonest.
Check items off as you finish them; add new items to the bottom of the phase they belong to.

Legend: `[ ]` open · `[x]` done · `[!]` blocked · sizes **S** (< 1 h) / **M** (half a day) / **L** (multi-day)

---

## Where the project is right now

| | |
| --- | --- |
| Model (`core/`) | complete: level parsing, maze rules, player, 4 ghosts, scoring, lives, mode schedule, events, fixed-timestep engine |
| Level data | one playable level, `levels/level1.txt` (21×21, 170 pellets), format documented |
| Tests | 81 tests, 10 CTest suites, all passing |
| Docs | `README.md` + `docs/` (architecture, code tour, gameplay rules, level format, testing) |
| View | **not started** — Qt 6 development packages are not installed on this machine |
| Android | **not started** |
| Git | **repository not initialised yet** |

Build and test, for reference:

```sh
cmake -S . -B build && cmake --build build -j && ctest --test-dir build --output-on-failure
./build/tests/pacman_tests Ghost      # one suite
```

---

## Phase 0 — housekeeping (do this first, S)

- [x] **Initialise a git repository** and commit the current green state, so every later
      experiment is revertible. Add a `.gitignore` for `build/`, `*.user`, `.cache/`,
      `*.apk`, `android/`, Qt Creator user files.
      *Done when:* `git log` shows one commit and `build/` is ignored.
- [ ] **Add a `README` badge-free build note**: one line stating that the only executable today
      is `build/tests/pacman_tests` (this already exists — delete this item if you prefer).
- [ ] Decide on a **code style** and apply it once: either accept the current style or add a
      `.clang-format` and run it over `core/` and `tests/` in a single formatting commit.
      *Done when:* `clang-format --dry-run -Werror` is clean.

## Phase 1 — close the core gaps (M, mostly test-driven)

These are the items flagged as "not covered yet" in `docs/testing.md`, plus the parser sharp
edge found while writing the docs. No new features, just correctness and proof.

- [ ] **Reject duplicate ghost markers** (`core/src/Level.cpp:221-249`). Today a second `2`
      silently wins over the first. Add the check next to the existing duplicate-`P` check and
      a `Level.rejectsDuplicateGhostMarker` test.
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
- [ ] **Per-power-pellet ghost counter**: eaten ghosts within one fright should escalate
      200 → 400 → 800 → 1600 per *chain*, not per ghost id. Currently the value depends only on
      the ghost id (`Ghost::pointsValue`, `Ghost.cpp:139`). Decide whether to keep the simple
      arcade-accurate-per-ghost table (documented in `docs/gameplay-rules.md`) or implement the
      chain, and make the docs match.
      *Note:* Current implementation gives points by ghost id (200/400/800/1600 by Blinky/Pinky/Inky/Clyde). Docs state this. Keeping as-is for now unless explicitly requested to change; decision needed.
- [ ] **Pixel-accurate collision** (optional, M): `Game::checkCollisions` currently compares
      tile indices (`Game.cpp:263`). A sprite-accurate version compares a small overlap box.
      Note that `docs/gameplay-rules.md` §7 documents the current behaviour, so this item needs
      a docs update too.
      *Status:* Deferred per current docs; not required now.
- [x] **Ghost house release timers** (partial, arcade-inspired): added per-ghost release based on pellets eaten (Blinky 0, Pinky 30, Inky 60, Clyde 90). Added `GhostMode::LeavingHouse`, `isReleased()`/`setReleased()`, and `Game::releaseGhostsIfReady()`. Behavior integrated into move loop; released state resets on level begin and when eyes return to spawn. Still a simplified model (does not implement full house exit pathing/LeavingHouse mode targeting deeply) — documented as intentional in Differences.
      *Status:* Core logic in place; LeavingHouse target set to above spawn (door area) but ghosts currently just move with normal AI until released in effect. Can refine further if needed.
- [ ] **Frightened warning flash**: ghosts turn white for the last ~2 s of a fright
      (`Ghost::chooseDirection`, `Ghost.cpp:181` — add a `FrightenedFlash` mode or a
      `flashWhite()` predicate) and the render-friendly state must be exposed for the view.
- [ ] **Corner turn restriction** (S): no upward turns in the four tiles directly above the
      ghost house door. Small rule, but it changes how the game feels; add a test either way.

## Phase 2 — content and progression (M)

- [ ] **Level list / `LevelManager`** (`core/`): a small class that owns a vector of levels,
      picks level *n* for `levelNumber_`, and tells the view the level name. Today
      `Game::beginLevel` replays the same map forever (`Game.cpp:87`).
- [ ] **Second real level** (`levels/level2.txt`) that is not a copy: fewer pellets, a tighter
      ghost house, a custom `modeSchedule`. Add it to the parse-everything test.
- [ ] **Difficulty curve** expressed in the level files: shorter scatter phases, faster ghosts,
      fewer frightened seconds for later levels. The knobs already exist in `LevelConfig`.
- [ ] **Fruit / bonus items** (M): a new `Tile` kind, a spawn timer in `Game`, points that
      depend on the level number, and a `FruitEaten` event. Touches `Tile.hpp`, `Level.cpp`
      (parsing), `Game.cpp` and `Events.hpp` — a good exercise of the layering.
- [ ] **Extra life thresholds** (S): e.g. 10 000 points → `+1` life, in `Game::killPlayer`'s
      sibling (`Game::collectPellet`) plus a test.

## Phase 3 — see the game without Qt (M) ← highest value before Phase 4

There is no renderer, so today the game cannot be watched. A tiny console renderer proves the
model is actually playable and becomes the reference the Qt view is compared against.

- [ ] **Add `tools/pacman_text`** (new CMake subdirectory, no Qt): a program that loads a level,
      runs `Engine` at a fixed rate, prints the maze as ASCII with pellets, and draws Pac-Man and
      the ghosts as characters (`@`, `B`, `P`, `I`, `C`), optionally with an event log line per
      frame. Read positions from `Game` only — no gameplay logic in this tool.
- [ ] **Keyboard input in the tool** (`read`/`poll` on stdin, arrow keys → `requestDirection`)
      to prove the input path a Qt view will use.
- [ ] **Add the reachability validator** as part of the tool or a test target: BFS the level for
      player-reachable pellets, ghost-reachable pellets and player-blocked house. This is the
      check that was done by hand for `level1.txt`.
- [ ] **Optional: an AI demo mode** where a scripted input sequence plays a whole level. Proves
      a level is completable.

## Phase 4 — the Qt Quick view (L, blocked)

**Blocker:** no Qt 6 development packages on this machine (`qmake6` missing). Nothing in the
model depends on this; the work starts the moment `qt6-base-dev`, `qt6-declarative-dev` and
`qt6-tools-dev` (plus `qml6-module-*` runtime modules) are installed.

- [ ] Install Qt 6 dev packages, then verify: `cmake --find-package` or a tiny
      `find_package(Qt6 REQUIRED COMPONENTS Quick)` project configures.
- [ ] **Add `app/`** with an option `PACMAN_BUILD_APP` (default `OFF` while Qt may be absent, so
      the core still builds anywhere): `qt_add_executable(pacman_app MANUAL_FINALIZATION)`,
      `qt_add_qml_module`, `Qt6::Quick`, `Qt6::QuickControls2`.
- [ ] **QML ↔ C++ boundary**: a small `GameController` QObject exposed with
      `Q_PROPERTY` (state, score, lives, levelName, frightenedSecondsLeft) and invokable slots
      (`requestDirection`, `start`, `reset`). This class is the only place where QML touches the
      model — keep all logic in `core/`.
- [ ] **Frame loop**: drive `Engine::update(delta)` from a `QQuickWindow`/`QQuickItem`
      `update()`/`afterAnimating` callback (or `QTimer` at ~60 Hz), and draw entities using
      `Engine::interpolationAlpha()` to interpolate between the previous and current positions.
- [ ] **Rendering**: first draw coloured rectangles per tile + circles for the actors (no assets)
      so the pipeline is proven; sprites (`pactex.png`, ghost sheet) come afterwards.
- [ ] **HUD**: score, high score, lives, level name, "READY!" and "GAME OVER" overlays, driven by
      `GameState` and `Event`s — not by polling for changes.
- [ ] **Audio**: a `SoundPlayer` that reacts to `Event`s (pellet, power pellet, ghost eaten,
      death). Keep all audio code in `app/`.
- [ ] **Test the view headlessly**: `QT_QPA_PLATFORM=offscreen` with `QTest`, asserting that a
      QML load succeeds and that a key event reaches `player().direction()`.
- [ ] **Input**: arrows + WASD, and a pause key. Later: swipe gestures for touch.

## Phase 5 — Android (L, blocked on Phase 4)

- [ ] Toolchain: `qt6-base-dev-tools`, Android SDK/NDK, and CMake with
      `-DCMAKE_TOOLCHAIN_FILE=$QT_DIR/lib/cmake/Qt6/android.toolchain.cmake`. Build with
      `qt_add_executable(... )` + `qt_deploy_runtime_dependencies`, producing an `.apk`.
- [ ] **Install on a device/emulator** and profile a frame: confirm 60 fps on a mid-range phone.
- [ ] **Touch input**: swipe to turn (with a dead zone so a tap does not spin), drag to move.
- [ ] **Lifecycle**: handle suspend/resume — the model should pause (`Engine` time scale 0) and
      resume without a giant `dt` spike; the existing frame clamp already helps.
- [ ] **Orientation and safe areas**: landscape-only, insets-aware layout.
- [ ] **Back button / immersive mode**: full-screen, back = pause menu.
- [ ] **Settings persistence** (sound, high score): the model has no file I/O by design, so this
      belongs to the view layer (or a separate `Storage` seam added to `core/` on purpose).
- [ ] **Touch-friendly level pacing**: same levels, maybe a reduced speed setting.

## Phase 6 — long-term quality (S/M, ongoing)

- [ ] **CI** (`.github/workflows/ci.yml`): configure, build with `-Werror`, run `ctest`, on
      Linux; later a job that builds the Qt app with `qt-qemu`/offscreen.
- [ ] **Coverage**: `gcovr` or `llvm-cov` report; aim to cover `Game.cpp` and `Entity.cpp`
      branches, then check the report into `docs/` as a baseline.
- [ ] **Fuzz the parser**: generate random header/map inputs and assert `parse` either succeeds
      or returns a non-empty error — never crashes, never throws.
- [ ] **Determinism regression**: a test that replays a recorded input log and compares a state
      hash (score, lives, positions, modes) against a golden value; re-generate the golden
      value only when a deliberate rule change happens.
- [ ] **Performance**: BFS per decision for eyes is fine at 21×21 but measure it; add caching if
      a bigger level ever needs it.
- [ ] **Install/export rules** for `pacman_core` so the Android app and any future tooling can
      `find_package(pacman_core)`.
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