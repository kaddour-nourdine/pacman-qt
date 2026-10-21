# pacman-qt

A Pac-Man clone written in modern C++ (C++17) that is meant to run on Linux **and** Android
from a single codebase.

The project follows a **model / view split**:

- **Model** (`core/`) — the game itself: the maze, Pac-Man, the ghosts, the rules, scoring,
  level parsing, the simulation clock. Pure C++, no graphics, no platform APIs, fully
  deterministic and unit-tested.
- **View** (`app/`) — a Qt Quick / QML front end that reads the model, draws the maze and
  actors in the classic arcade style on a `Canvas` (blue outlined walls, wedge Pac-Man with an
  animated mouth, dome ghosts, WAV sound effects) and forwards key presses. A small
  `GameController` QObject is the only bridge between QML and the model, so `core/` stays
  Qt-free and can be reused on Android unchanged.

---

## Status

| Part | State |
| --- | --- |
| Simulation model (maze, player, ghosts, rules, level list, clock) | Done, 139 tests passing |
| Level file format, level list (`LevelManager`) + three playable levels (`levels/level1.txt` is the classic arcade maze, `levels/level2.txt`, `levels/level3.txt`) | Done |
| Qt Quick / QML view, window, input (`app/`, needs Qt 6) | Done (classic vector arcade rendering, HUD, sound effects, pause menu) |
| Headless terminal renderer (`tools/pacman_text`) | Done |
| Automated headless Qt view test (`tests/test_view.cpp`, `PACMAN_BUILD_QT_TESTS`) | Done (loads QML and checks a key event reaches the controller; 11 cases: input, back key, pause-menu entries, Android bridge, suspend/resume, settings, slow mode, audio) |
| Android packaging | Build pipeline complete — `./build-android.sh` cross-compiles the Qt app for arm64-v8a and produces a signed APK |
| Android on-device | Verified on a phone (Tecno A059, Android 16): touch controls, audio (persistent-stream mixer), bundled levels, Pac-Man launcher icon |
| Android lifecycle / layout | Implemented and verified on a phone — free orientation + immersive mode, safe-area insets, back button = pause menu (own activity), suspend/resume without a time spike, persistent sound/speed settings |
| CI | `.github/workflows/ci.yml` — core + Qt jobs on Linux, build with `-Werror`, `ctest` |

The model runs on its own: `Game.playsRealLevelEndToEnd` plays a level for ten simulated
seconds and checks that pellets get eaten and the ghosts leave their house. To watch it without
Qt, run the terminal renderer; to run the real view, build the Qt app (below).

---

## Build and test

Requires CMake ≥ 3.16 and a C++17 compiler. Nothing else — no external libraries.

```sh
cmake -S . -B build                    # configure (defaults to RelWithDebInfo)
cmake --build build -j                 # compile
ctest --test-dir build --output-on-failure   # run all tests
```

Useful variations:

```sh
cmake -S . -B build -DPACMAN_BUILD_TESTS=OFF   # build the core library only
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug   # with debug info and no optimisation
cmake -S . -B build -DPACMAN_BUILD_APP=ON      # also build the Qt Quick app (needs Qt 6)
cmake -S . -B build -DPACMAN_BUILD_APP=ON -DPACMAN_BUILD_QT_TESTS=ON   # also build the headless Qt test
./build/tests/pacman_tests            # run every test, pretty output
./build/tests/pacman_tests Ghost      # run one suite
./tools/coverage.sh                   # coverage report of the model (needs gcovr)
./build/tools/pacman_bench/pacman_bench  # performance baseline (optional, run by hand)
```

`pacman_tests` prints one line per test (`ok` / `FAIL`) and returns a non-zero exit code if
anything failed, so it also works as a plain command-line tool.

### The Qt app

`app/` is built only with `-DPACMAN_BUILD_APP=ON` (default `OFF`, so the core still builds on a
machine without Qt). It needs the Qt 6 development packages for **Quick**,
**QuickControls2** and **Multimedia** (the latter for the audio player), e.g. on Fedora:

```sh
sudo dnf install qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtmultimedia-devel
cmake -S . -B build -DPACMAN_BUILD_APP=ON && cmake --build build -j
./build/app/pacman_app
```

Controls: arrow keys / WASD to steer, **Space** to start or restart, **P** to pause,
**Esc** (or the Android back button) to open the pause menu, **M** to mute, **R** to reset.
On a touch screen: swipe to steer, tap to start/restart, and the `≡` button in the HUD opens
the pause menu. For a machine with no display (CI), run it with
`QT_QPA_PLATFORM=offscreen`.

The headless view test (`tests/test_view.cpp`) is built only with `-DPACMAN_BUILD_QT_TESTS=ON`
(which also requires `PACMAN_BUILD_APP=ON`) and needs the Qt 6 **Test** component. It is
registered as the `QtView` CTest test and runs under `QT_QPA_PLATFORM=offscreen`: it loads
`Main.qml` and checks that a key event sent to the window reaches the controller, plus the
pause menu (back key and the Android bridge), suspend/resume, the persisted settings and the
slow mode.

---

## Repository layout

```
CMakeLists.txt              top-level project: language standard, options, sub-projects
core/                       the model library (no graphics, no Qt)
  CMakeLists.txt            builds the static library pacman_core (alias pacman::core)
                            and installs it with a find_package() package config
  include/pacman/           public headers, one per concept
  src/                      implementations
app/                        the Qt Quick view (built only with -DPACMAN_BUILD_APP=ON)
  CMakeLists.txt            builds the pacman_view library + the thin pacman_app executable
  main.cpp                  QGuiApplication + QQmlApplicationEngine entry point
  GameController.*          the only bridge between QML and the model
  AppSettings.*             persistent user settings (sound, slow mode)
  ScreenInsets.*            the safe area (notch, system bars) for the layout
  AndroidBridge.*           JNI bridge for the Android back button
  Main.qml                  the maze, HUD, overlays, pause menu, input and sound effects
  assets/                   SVG sprites, the app icon, and WAV sound effects (embedded as Qt resources)
android/                    the Android package: manifest, activity and launcher icon
  package/                  QT_ANDROID_PACKAGE_SOURCE_DIR (manifest + our activity + res/)
  icon/                     the launcher-icon artwork
tools/
  pacman_text/              a terminal renderer you can actually play
  ai_demo/                  a small scripted run of the engine
  pacman_fuzz/              a fuzzer for the level parser
  pacman_bench/             measures path search and simulation throughput
  coverage.sh               builds with coverage counters and reports via gcovr
levels/
  level1.txt, level2.txt, level3.txt  the playable levels, written in a plain ASCII
                                      format (also embedded as Qt resources so an
                                      installed app or phone can load them)
tests/                      the test suite (139 tests, no external framework)
  test_view.cpp             optional headless Qt view test (PACMAN_BUILD_QT_TESTS)
  install_check.cmake       proves that pacman_core installs and is found again
  install_consumer/         a project outside the tree, built against the installed package
docs/                       the documentation you are reading
```

Inside `core/include/pacman/` the headers form a strict bottom-up order — each one only
includes the ones above it:

```
Vec2.hpp      a 2D point                       (no dependencies)
Direction.hpp four directions + helpers        (no dependencies)
Tile.hpp      what a single maze tile is        (no dependencies)
Random.hpp    seeded random numbers             (no dependencies)
Events.hpp    the "something happened" messages
Level.hpp     parsed level file  →  Level
LevelManager.hpp  the campaign: list of levels      uses Level
Maze.hpp      the playable grid + pellet state       uses Level
Entity.hpp    anything that walks on the grid        uses Maze
Player.hpp    the player                             uses Entity
Ghost.hpp     a ghost + its AI                       uses Entity
Game.hpp      the whole game: rules, score, states   uses Maze, Player, Ghost
Engine.hpp    turns real time into fixed steps       uses Game
```

---

## Suggested reading order

1. [`docs/architecture.md`](docs/architecture.md) — the big picture: layers, one frame of
   simulation, the design decisions and *why* they were made.
2. `core/include/pacman/Vec2.hpp`, `Direction.hpp`, `Tile.hpp` — the three tiny value types
   everything else is built from.
3. `core/src/Level.cpp` — how a text file becomes a `Level`.
4. `core/src/Maze.cpp` — walls, doors, tunnels, pellet bookkeeping, path finding.
5. `core/src/Entity.cpp` — the movement algorithm shared by the player and the ghosts.
   Read this one twice; it is the heart of the engine.
6. `core/src/Player.cpp` then `core/src/Ghost.cpp` — the two characters.
7. `core/src/Game.cpp` — the rules of the game (pellets, collisions, lives, modes).
8. `core/src/Engine.cpp` — the fixed-timestep clock.
9. [`docs/testing.md`](docs/testing.md) and then the `tests/` folder.

Supporting documents:

- [`docs/gameplay-rules.md`](docs/gameplay-rules.md) — the Pac-Man rules as they are actually
  implemented here, mapped to the code, plus the places where this implementation knowingly
  differs from the 1980 arcade original.
- [`docs/level-format.md`](docs/level-format.md) — the level file format, so you can design
  your own levels.
- [`docs/code-tour.md`](docs/code-tour.md) — a file-by-file tour with the responsibility of
  every class and function.
- [`docs/testing.md`](docs/testing.md) — how the test harness works and how to add a test.
- [`docs/coverage.md`](docs/coverage.md) — the coverage baseline of the model (`./tools/coverage.sh`)
  and why every branch no test takes cannot be reached.
- [`docs/performance.md`](docs/performance.md) — the performance baseline for the path search and
  the simulation (`./build/tools/pacman_bench/pacman_bench`).
- [`docs/view-implementation.md`](docs/view-implementation.md) — complete documentation of the
  Qt Quick view (Phase 4), including architecture, components, design decisions, and testing.

---

## The model in 60 seconds

A few ideas explain most of the code:

1. **The world is a grid of tiles.** Tile `(x, y)` covers the square from `(x, y)` to
   `(x+1, y+1)`, so its centre is at `(x + 0.5, y + 0.5)` (`tileCenterOf`).
2. **Positions are continuous floats, decisions are per tile.** An entity carries a position
   like `12.34, 5.5` and moves smoothly, but it may only *change direction* when it is exactly
   on a tile centre. That is why Pac-Man can turn "just after" a wall corner the way the
   arcade does, and why everything about the grid stays on whole numbers.
3. **Speed is measured in tiles per second** (`playerSpeed = 6.0` means six tiles per second).
4. **The simulation always advances in fixed 1/120 s steps** (`Engine`). Real frame times are
   variable, so `Engine` accumulates them and calls `Game::step(1/120)` as many times as
   needed. Same inputs → same result, on any machine, at any frame rate.
5. **The view never writes game rules.** It sends `requestDirection(...)` and reads positions,
   scores, states and *events* (`PelletEaten`, `GhostEaten`, `PlayerDied`, …) that the model
   emits. A view can be replaced without touching a single line of `core/`.
6. **Nothing random depends on the clock.** The only randomness is a small seeded generator
   (`Random`), so a given seed replays the exact same game — which is what makes the tests
   reliable.

---

## Rules for adding code

Keep these so the model stays usable from Qt (and from Android):

- `core/` must not include any Qt, OpenGL, windowing or platform header. No `printf`, no
  reading the system clock, no files other than level files (`Level::loadFile` and
  `LevelManager::loadDirectory`).
- Game rules belong in `Game`, not in the view. If the player cannot enter the ghost house,
  that is a `Maze::canEnter` question; if a ghost should chase, that is `Ghost::chaseTarget`.
- Anything the view may want to react to should be reported through `Event`
  (`pushEvent` / `drainEvents`) instead of the view having to poll for it.
- Never use wall-clock time in the model. Take the elapsed time as a parameter.
- Add a test with every change; `ctest` must stay green.