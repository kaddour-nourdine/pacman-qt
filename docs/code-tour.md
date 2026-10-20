# Code tour

Every file in the repository, in dependency order, with the responsibility of each class and
function. Read [`architecture.md`](architecture.md) first for the "why".

Line numbers refer to the current state of the files and are there to help you jump around;
function names are the reliable anchor.

---

## Build files

### `CMakeLists.txt`

Top-level project. Sets C++17 (`CMAKE_CXX_EXTENSIONS OFF` means strict standard mode),
defaults to `RelWithDebInfo`, defines `PACMAN_BUILD_TESTS`, and adds `core/` then `tools/`.
It also defines `PACMAN_BUILD_APP` (default `OFF`, so a machine without Qt still builds the
model) and, when it is `ON`, adds `app/` **before** `tests/` — the optional Qt view test links
the view library defined in `app/`, so `app/` must be processed first. `tests/` is added last
and calls `enable_testing()`.

### `core/CMakeLists.txt`

Builds the static library `pacman_core` and the alias `pacman::core`. `include/` is a `PUBLIC`
include directory, so anything linking `pacman::core` automatically sees `pacman/*.hpp`.
`POSITION_INDEPENDENT_CODE ON` is set because a static library that will later be linked into
a shared library or an Android `.so` must be position independent. Warnings
`-Wall -Wextra -Wpedantic` are enabled on GCC/Clang.

### `tests/CMakeLists.txt`

Builds one executable `pacman_tests` from all `Test*.cpp` files and registers **one CTest test
per suite** with a `foreach` loop. It also builds `pacman_tests_reach` (the separate
reachability gate from `TestReachability.cpp`) and passes `PACMAN_LEVELS_DIR` as a compile
definition so the tests can find `levels/level1.txt`. The optional `PACMAN_BUILD_QT_TESTS`
option (only if `PACMAN_BUILD_APP` is `ON`) builds `pacman_qttest` from `test_view.cpp` and
registers it as the `QtView` CTest test with `QT_QPA_PLATFORM=offscreen` in its environment.

> If you add a new suite name, add it to that `foreach` list, otherwise the test will run when
> you execute `pacman_tests` manually but CTest will never see it.

### `tools/CMakeLists.txt`

Adds the two console tools: `pacman_text` (the playable terminal renderer) and `ai_demo`
(a scripted run of the engine). Both link only `pacman_core`, so neither pulls in Qt.

### `app/CMakeLists.txt`

Only processed when `PACMAN_BUILD_APP=ON` at the top level. It finds `Qt6 Quick` and
`QuickControls2` and builds **two targets**:

- `pacman_view` — a static library holding `GameController` plus the QML module. It attaches
  `Main.qml` with `qt_add_qml_module(URI PacMan ... NO_PLUGIN)` (there are no C++-registered
  QML types, so no plugin is needed) and embeds the SVG sprites and WAV sounds with
  `qt_add_resources(... pacman_assets ...)`. The old `app/assets.qrc` file is gone, so a new
  asset only needs a line in that `FILES` list. `PACMAN_LEVELS_DIR` is passed as a compile
  definition so the controller can load the campaign.
- `pacman_app` — a thin `qt_add_executable` wrapping only `main.cpp`, linking `pacman_view`.

Keeping the view in a library lets the headless test (`tests/test_view.cpp`) link exactly the
same code the app uses, instead of trying to link the executable.

---

## Layer 1 — value types

### `core/include/pacman/Vec2.hpp`

A plain `struct Vec2 { float x; float y; }` with `constexpr` operators. No constructors, no
inheritance: it is a value type.

| Symbol | Meaning |
| --- | --- |
| `operator+ - * /` | arithmetic, both orders for `*` |
| `operator== !=` | exact comparison (used by tests) |
| `length`, `lengthSquared`, `distance` | Euclidean helpers |
| `nearlyEquals(a, b, eps = 1e-4)` | float-tolerant comparison for tests |
| `tileCenterOf(x, y)` | `→ {x + 0.5f, y + 0.5f}`, the centre of tile `(x, y)` |
| `tileIndexOf(v)` | `floor(v)`, the tile index a float coordinate falls in |

The two tile helpers are the bridge between the continuous world and the grid; almost every
`tileX()` in the codebase is one of them.

### `core/include/pacman/Direction.hpp`

`enum class Direction { None, Up, Right, Down, Left }` plus:

- `kDirections[] = {Up, Right, Down, Left}` — a natural clockwise order, used where order is
  irrelevant (BFS neighbour expansion).
- `kGhostPreference[] = {Up, Left, Down, Right}` — **the tie-break order of the arcade game**
  (Up, Left, Down, Right). Because ghost selection compares distances with a strict `<`, the
  first entry wins a tie.
- `opposite`, `dirX`, `dirY`, `directionFromDelta`, `isHorizontal`, `isVertical`, `toString`.

Everything is `constexpr`, so these can be used in compile-time contexts and constant arrays.

### `core/include/pacman/Tile.hpp`

`enum class Tile : std::uint8_t { Empty, Wall, Pellet, PowerPellet, Door, Tunnel, Fruit }` and the
predicates the rest of the code uses instead of comparing enum values directly:
`isWall`, `isWalkable`, `blocksGhost`, `isDoor`, `isTunnel`, `hasPellet`, `isPowerPellet`, `isFruit`.

It also owns the **text ↔ tile mapping**: `tileToChar`, `charToTile`, `isMapChar`,
`isGhostMarker`. Keeping this in one header means the level parser, a future level editor and a
future renderer all agree on the alphabet.

### `core/include/pacman/Random.hpp`

A 40-line xorshift32 generator. `next()` is the bit-twiddle, `unit()` gives `[0, 1)`,
`range(lo, hi)` gives an inclusive integer range. A seed of `0` is replaced with `1`, because
xorshift cannot escape the all-zero state.

It exists so that frightened-ghost randomness is **seedable and reproducible**. Swapping in
`std::mt19937` later would be a drop-in change.

### `core/include/pacman/Events.hpp`

`EventType` (`LevelStarted`, `PelletEaten`, `PowerPelletEaten`, `FruitEaten`, `GhostEaten`,
`GhostRespawned`, `PlayerDied`, `LevelCompleted`, `GameOver`) and the `Event` struct:
`type`, `x`, `y`, `points`, `value`.

`x/y` are tile coordinates (or `-1`), `points` the score gained, `value` whatever the event
needs — the level number, the ghost id, the remaining lives, the total score. Events are the
model's only way of telling the view that something happened.

---

## Layer 2 — level content

### `core/include/pacman/Level.hpp`

```cpp
struct LevelConfig {          // the "tuning knobs" of a level
    float playerSpeed = 6.0f; float ghostSpeed = 5.0f;
    float frightenedSpeed = 3.5f; float tunnelSpeed = 4.0f; float eyesSpeed = 16.0f;
    float frightenedSeconds = 6.0f; int startingLives = 3;
    std::vector<float> phaseDurations;   // empty ⇒ use the default schedule
};

struct GhostSpawn { int x, y; Direction direction = Direction::Left; bool fromMap = false; };

class Level {
    static bool parse(const std::vector<std::string>& lines, Level& out, std::string* error);
    static bool loadFile(const std::string& path, Level& out, std::string* error);
    Tile tileAt(int x, int y) const;      // out of bounds ⇒ Wall
    int playerX/playerY/playerDirection, const GhostSpawn& ghostSpawn(int i), const LevelConfig& config();
};
```

`Level` is **immutable data**: after parsing, nothing ever changes it. `ghostSpawn()` returns a
static fallback for out-of-range indices so callers never have to bounds-check.
`Level::kGhostCount` is `4`.

### `core/src/Level.cpp`

Two phases.

*Parsing the header* — `applyHeaderEntry` (line 73) maps a key to a config field with strict
conversion: `parseFloat`/`parseInt` use `std::from_chars` and require the entire string to be
consumed, so `6.0x` or `abc` are errors rather than silently `0`. Positive-value checks are
next to each key, which keeps the error messages specific
(`invalid value for 'playerSpeed': 'x'`). Unknown keys fail loudly. `from_chars` is
deliberately used instead of `strtof`/`strtol`: the latter honour the C locale, and because a
Qt application installs the user's locale (`fr_FR` uses a decimal comma) a level could fail to
load depending on the machine. See the regression test
`Level.parsesIndependentlyOfNumericLocale`.

*Parsing the map* — `Level::parse` (line 159) scans rows in order, treating the first
non-`key = value` line as the start of the map. It enforces equal row widths, valid
characters, exactly one `P`, no blank line inside the map, and then walks the grid once,
recording spawn points and counting pellets. `1`–`4` markers are *not* stored as tiles: the
tile becomes `Empty` and only the spawn coordinate is kept. Ghosts without a marker get the
first walkable non-player tile (`Level.cpp:285`).

`loadFile` is a thin wrapper: read the file into a vector of lines, call `parse`.

Nothing throws; both entry points return `bool` and fill an error string. That is deliberate:
a level file is user input, and the view will want to show the error message.

### `core/include/pacman/LevelManager.hpp`

```cpp
class LevelManager {
    LevelManager() = default;
    explicit LevelManager(std::vector<Level> levels);
    bool add(Level level);                                  // false if the level is not valid
    bool loadFile(const std::string& path, std::string* error);
    bool loadDirectory(const std::string& directory, std::string* error);
    void clear();
    int levelCount() const;  bool isEmpty() const;  const std::vector<Level>& levels() const;
    const Level& levelFor(int levelNumber) const;           // 1-based, wraps past the end
    const std::string& nameFor(int levelNumber) const;      // the label for the view
};
```

The campaign: an ordered list of levels plus the rule that turns a *level number* into a
level. `levelFor` is 1-based and wraps — `levelFor(3)` with two levels is the first one again,
which is what the arcade does with its campaign. Numbers below 1 clamp to the first level, and
an empty manager answers with a static invalid `Level` and an empty name, so callers never
have to null-check.

### `core/src/LevelManager.cpp`

`loadFile` is `Level::loadFile` plus an append. `loadDirectory` is the interesting one: it
collects every `*.txt` in the directory, **sorts it by file name** (directory iteration order
is unspecified and the model must be deterministic), parses all of them into a temporary list,
and only appends the result when every file parsed — one broken file anywhere in `levels/`
means nothing is loaded, and the error message names the offending file first. Non-`.txt`
files are ignored; a missing or empty directory is an error.

`add` refuses an invalid level, so an unparsed one cannot poison the list. This is the only
directory I/O in `core/`; everything else still only ever reads a single level file.

---

## Layer 3 — the board

### `core/include/pacman/Maze.hpp`

```cpp
explicit Maze(Level level);          // takes ownership of the level data
Tile tileAt(int x, int y) const;     // tunnel-aware
bool canEnter(int x, int y, bool passDoors) const;
bool isTunnel/isPellet(int x, int y) const;
int wrapX(int x, int y) const;       // wraps only at tunnel tiles
int tileXOf/tileYOf(Vec2) const; Vec2 wrapPosition(Vec2) const;
Tile takePellet(int x, int y);       // removes and returns the pellet (Empty if none)
int pelletsRemaining/pelletsTotal/pelletsEaten() const; bool allPelletsEaten() const;
void resetPellets();
Direction stepToward(int fromX, int fromY, int toX, int toY, bool passDoors) const;
const Level& level() const;          // entities read their own spawn data from here
```

### `core/src/Maze.cpp`

The constructor copies the level's grid into `base_` (pristine) and calls `resetPellets()`,
which copies `base_` into `tiles_` (live) and counts the pellets. Two grids is what makes
"restart this level" a single cheap copy (`Maze.cpp:22`).

Key functions:

- `rawTile` — the raw grid lookup that treats out-of-bounds as wall. `tileAt` is the *public*
  version that applies tunnel wrapping first; internally `wrapX` uses `rawTile` to avoid
  infinite recursion.
- `wrapX` (line 46) — if `x` is off the board, look at the edge tile of that row: only a
  `Tunnel` permits wrapping, otherwise the coordinate stays off-grid and callers see a wall.
  Vertical off-grid never wraps.
- `canEnter` (line 78) — the single authority on "may this thing stand on that tile?". Walls
  never. Doors only when `passDoors`.
- `takePellet` (line 117) — returns the tile that was removed, or `Empty` if there was nothing
  to remove. Returning the tile (instead of a bool) is what lets `Game` know whether it was a
  power pellet, and it is inherently idempotent: the second call finds nothing.
- `stepToward` (line 141) — a breadth-first search from `(fromX, fromY)` to the goal, wrapping
  horizontally and honouring `passDoors`, recording the direction that first reached each tile
  in `parentDirection`. It then walks the parent chain back and returns **only the first step**.
  If start equals goal, or the goal is unreachable, it returns `Direction::None`.

BFS is used because eaten ghosts must take a *shortest* path home — a greedy "move towards the
target" rule can get stuck behind the house walls. Note that it runs per decision while a ghost
is in `Eyes` mode, which is affordable at the grid sizes involved but would be worth caching
for a much larger level.

---

## Layer 4 — movement

### `core/include/pacman/Entity.hpp`

The base class for anything that walks on a grid. It owns `maze_` (a pointer, non-owning),
`position_`, `direction_`, `requestedDirection_`, `speed_` and `distanceTravelled_`.

Public surface: `position/setPosition`, `direction/setDirection`,
`requestDirection/clearRequestedDirection`, `speed/setSpeed`, `tileX/tileY/tileCenter`,
`isAtTileCenter`, `isBlocked`, `distanceTravelled`, `canMove`, `reverse`, `update`.

Three **protected virtual hooks** are where subclasses plug in:

| Hook | Default | Purpose |
| --- | --- | --- |
| `onArriveAtCenter()` | empty | react to reaching a centre (ghosts reverse when frightened) |
| `chooseDirection()` | keep going straight, else stop | decide the next direction |
| `canPassDoors()` | `false` | the player/ghost difference |

Copying is deleted and moving is defaulted (`Entity.hpp:15-18`). That is not cosmetic: `Game`
stores ghosts in a `std::vector<Ghost>`, and `Game` holds a reference to the `Maze` while the
ghosts hold raw pointers to it — so copying a ghost would be wrong, but moving one (which
reallocates the vector) must stay possible.

### `core/src/Entity.cpp`

`update(deltaSeconds)` (line 135) is the algorithm to understand. It converts the frame into
`remaining = dt × speed` (a distance in tiles) and then loops:

```
while there is distance left:
    if we are NOT on a tile centre:
        step forward by min(remaining, distanceToNextCenter)
        if we just landed exactly on a centre → snapToNextCenter()
    else:
        onArriveAtCenter()                 // subclass hook
        chosen = chooseDirection()         // subclass hook
        if nothing legal → direction = None, stop
        step toward the next centre, snap when it fits exactly
```

The loop (rather than a single jump of `dt × speed` tiles) is what makes big time steps behave
like many small ones. The `guard < 512` counter is a safety valve so a pathological input
cannot spin forever. Supporting members:

- `distanceToNextCenter` — the distance to the centre of the *next* tile in the current
  direction. Working with the next centre rather than the current one means an entity that is
  exactly on a centre immediately starts a full tile of movement.
- `snapToNextCenter` — pins the moving axis to `tile + 0.5`, killing accumulated float error.
- `advance` — adds `dirX/dirY × distance`, wraps through tunnels, accumulates
  `distanceTravelled_`.
- `canMove(direction)` — asks the maze whether the neighbouring tile in that direction is
  enterable. `reverse()` flips direction if the reverse is legal.

---

## Layer 5 — the characters

### `Player` (`core/include/pacman/Player.hpp`, `core/src/Player.cpp`)

Thin by design: `reset()` restores the level's start tile, direction and speed and zeroes the
counters; `notePelletEaten(Tile)` increments the pellet or power-pellet counter;
`animationPhase()` returns `fmod(distanceTravelled × 0.5, 1)`, i.e. two mouth cycles per tile,
purely so a renderer does not have to reinvent distance-based animation.

### `Ghost` (`core/include/pacman/Ghost.hpp`, `core/src/Ghost.cpp`)

`GhostId` (Blinky…Clyde) and `GhostMode` (Scatter, Chase, Frightened, Eyes) with `toString`
helpers. The class adds identity (`id_`), spawn data (`spawnX_/spawnY_/spawnDirection_`), the
current `mode_`, the last known `target_`, and a **non-owning pointer to the shared
`Random`** so frightened choices are seeded by the game.

- `updateTarget(playerPos, playerDir, blinkyPos)` (line 123) recomputes `target_` from the
  mode. It is called by `Game` every step, before `update`, so the AI always aims at a fresh
  snapshot rather than reading the player's fields itself.
- `chaseTarget` (line 90) is the four-personality switch documented in
  [`gameplay-rules.md`](gameplay-rules.md) §4. Note it is a pure function of its three
  arguments, which is why `TestGhost` can test every personality without building a game.
- `chooseDirection` (line 199) implements the selection rules: eyes ask `stepToward`,
  frightened asks `frightenedDirection`, otherwise pick the legal neighbour closest to the
  target with Up/Left/Down/Right tie-breaking. `onArriveAtCenter` reverses frightened ghosts.
- `eat()` sets `Eyes`, `hasReachedSpawn()` reports arrival home, `reset()` returns the ghost to
  a scatter-mode life at its spawn.
- `setMode` reverses the ghost when it enters Frightened or Eyes — the classic "the ghosts all
  turn around" moment.

`Random*` is a pointer rather than a value so that all four ghosts share one deterministic
stream; `frightenedDirection()` falls back to the first legal option when no generator is
attached, which keeps `Ghost` usable standalone in tests.

---

## Layer 6 — the rules

### `ModeSchedule` (`core/include/pacman/Game.hpp:31`)

A list of phase durations plus `modeAt(seconds)`, which walks the list subtracting durations
and returns `Scatter` for even phases and `Chase` for odd ones. The default list is
`7,20,7,20,5,20,5,20` (`Game.cpp:27`). Constructed from `LevelConfig::phaseDurations`, so a
level file can override it; `usesCustomSchedule()` lets a future view say so.

### `Game` (`core/include/pacman/Game.hpp`, `core/src/Game.cpp`)

The orchestrator. Members: `levels_` (the campaign, a `LevelManager`), `level_` (the level
being played), `maze_`, `player_`, `std::vector<Ghost> ghosts_`, `random_`, `schedule_`, the
state, the score/lives/level counters, the two timers and the event queue.

Constants: `kPelletPoints = 10`, `kPowerPelletPoints = 50`, `kFruitPoints = 100`, `kDeathPauseSeconds = 1.6f`,
`kLevelCompletePauseSeconds = 2.0f`.

Lifecycle methods:

| Method | What it does |
| --- | --- |
| `Game(Level)` (line 66) | wraps a single level in a one-entry `LevelManager` and delegates |
| `Game(LevelManager)` (line 69) | takes the campaign **by value** — `Game` owns its copy, so nothing can dangle — then builds `level_` from `levelFor(1)`, the maze, the player, the ghosts and the schedule, and calls `beginLevel()` |
| `createGhosts()` (line 80) | (re)builds the four ghosts from `level_`'s spawn markers |
| `loadLevel(const Level&)` (line 89) | swaps the current level: reassigns `level_`, rebuilds `maze_`, `player_`, the ghosts and `schedule_`. Score and lives are deliberately untouched |
| `reset()` (line 97) | new game: score 0, `levelNumber_ = 1`, `loadLevel(levelFor(1))`, full lives, `beginLevel()` |
| `beginLevel()` (line 105) | empty level list → `endGame()`; otherwise pellets, player and ghosts reset, timers and mode cleared, state `Ready`, emits `LevelStarted` |
| `start()` (line 133) | `Ready → Running` |
| `step(dt)` (line 139) | the only clock-facing method: simulate while `Running`, otherwise run the pause timers |
| `simulate(dt)` (line 177) | fright timers → player → pellets → ghosts → collisions → level complete check |
| `drainEvents()` (line 426) | hand the accumulated events to the caller, leaving an empty queue |

**Level progression.** When the `LevelComplete` pause expires, `step` does `++levelNumber_`,
`loadLevel(levels_.levelFor(levelNumber_))`, `beginLevel()` (`Game.cpp:171`). With a one-level
manager — what `Game(Level)` builds — `levelFor` wraps, so the same map replays forever. With a
full list the game walks the campaign and wraps at the end. Lives and score carry over between
levels: `startingLives` is applied only by the constructors and by `reset()`, while speeds and
the mode schedule always come from the level that is now loaded. An empty list means the game
reports `GameOver` immediately instead of building an unplayable board.

`levelName()` is `levels_.nameFor(levelNumber_)` — the label the view shows for the current
level, and `""` when there is none. `levelManager()` hands the campaign to a level-select
screen.

The private helpers are small and single-purpose: `createGhosts`, `loadLevel`, `beginLevel`,
`updateFrightened`, `applyModes`, `movePlayer`, `moveGhosts`, `releaseGhostsIfReady`,
`collectPellet`, `checkCollisions`, `killPlayer`, `finishLevel`, `endGame`, `speedFor`,
`pushEvent`. Read them in that order — it is the order in which they affect the game.

`Game` is non-copyable (`Game.hpp:57`) because it hands out references to its own members;
moving it would invalidate every entity's `maze_` pointer.

Accessors: `state`, `score`, `lives`, `levelNumber`, `levelName`, `levelManager`,
`globalMode`, `isFrightened`, `frightenedSecondsLeft`, `pelletsRemaining`, plus `maze()`,
`player()` and `ghosts()` in both const and non-const form. The non-const overloads are the
seam tests use to teleport the player or a ghost; a renderer can use them for interpolation as
well.

---

## Layer 7 — time

### `Engine` (`core/include/pacman/Engine.hpp`, `core/src/Engine.cpp`)

```cpp
static constexpr double kStepSeconds = 1.0 / 120.0;
int update(double frameSeconds);              // returns how many fixed steps ran
void setTimeScale(double); void setMaxFrameSeconds(double);
double interpolationAlpha() const;            // 0..1, how far into the next step we are
double simulatedSeconds() const; std::uint64_t stepCount() const;
```

`update` (line 16) multiplies the frame by `timeScale_`, clamps it to `maxFrameSeconds_`
(default `0.25`) so that a long stall — a breakpoint, a dragged window, a laptop suspend —
cannot trigger thousands of steps, adds it to the accumulator, and runs whole `kStepSeconds`
steps while it can.

It returns the number of steps so a caller (a benchmark, a test) can react. `stepCount_` and
`simulatedSeconds_` exist for tests that need a deterministic "how much time has passed"
without touching the model.

`interpolationAlpha()` is the leftover fraction of a step. The model never interpolates
itself, but a renderer can draw each entity between its previous and current position to stay
smooth at any refresh rate.

---

## The tools (`tools/`)

Neither tool contains gameplay logic: they drive `Engine` and only read `Game`, which is
exactly what the Qt view does. They exist so the model can be watched and fuzzed without Qt.

### `tools/pacman_text/main.cpp` (236 lines)

A terminal renderer you can actually play. It loads the whole `levels/` directory with
`LevelManager::loadDirectory` (or a single file given on the command line), runs `Engine` at
a real-time rate, and redraws the board in place with ANSI escape codes: walls, pellets,
fruit, Pac-Man (`@`) and the ghosts (`B`, `P`, `I`, `C`), plus the score/lives/state line.
Arrow keys and WASD become `requestDirection` calls; `Q` quits. The terminal is switched to
raw mode (POSIX `termios`, with a Windows `conio` path behind `#ifdef _WIN32`) and restored
on exit, including on `SIGINT`. `PACMAN_LEVELS_DIR` is a compile definition here too, so the
tool finds the levels without being told where the source tree is.

### `tools/ai_demo/ai_demo.cpp`

A scripted sanity run: load `levels/level1.txt`, start the game, step `Engine` at 5× real
time for 10 000 frames with no input, and print `OK` (or `OVER`). It is a smoke test a human
can read, and it complements `TestGame.playsRealLevelEndToEnd` by running much longer.

---

## The application (`app/`)

Built only when `PACMAN_BUILD_APP=ON`. Uses Qt 6 Quick, QuickControls2 and Multimedia
(`qt_add_library(pacman_view)` + `qt_add_qml_module` + `qt_add_resources`, then a thin
`qt_add_executable(pacman_app)`); the QML module URI is `PacMan` and `Main.qml` lives directly in
`app/` (a flat layout avoids needing a `qmldir`). The QML module is built `NO_PLUGIN` because it
has no C++-registered types.

### `app/main.cpp`

`QGuiApplication` + `QQmlApplicationEngine`. Sets the organisation/application names (`pacman-qt`
/ `pacman`) before constructing the controller, because `QSettings` in the controller reads
them. Creates the `GameController` on the stack, exposes it as the context property `"game"`,
connects `objectCreationFailed` (a direct connection, so it fires before the event loop) and
`warnings` to `qWarning`, then `loadFromModule("PacMan", "Main")`. If no root object was
created it returns `-1`, so a broken QML file fails the process instead of running a blank
window.

### `app/GameController.hpp` / `app/GameController.cpp`

The only class that includes both Qt and `core/`. It owns a `Game` (built from
`LevelManager::loadDirectory(PACMAN_LEVELS_DIR)`) and an `Engine`, and it is where real time
enters the program:

- **Properties** (read by QML, change-notified by signals): `score`, `highScore`, `lives`,
  `levelNumber`, `levelName`, `pelletsRemaining`, `stateName`, `paused`, `mazeData`, `itemData`,
  `mazeCols/Rows`, `playerX/Y`, `playerMouth`, `playerDirection` and `ghosts`.
- **Invokable slots**: `start()`, `reset()`, `togglePause()` and `requestDirection(int)` — a
  thin translation of the four `Direction` values. Nothing here re-implements a rule. A
  test-only accessor `directionForTest()` exposes the player's queued (requested) direction to
  the headless QTest, which uses it to confirm a key event arrived.
- **Frame loop**: a 16 ms `Qt::PreciseTimer` calls `onTick`, which measures real time with a
  `QElapsedTimer` and passes it to `Engine::update`. Before each update it snapshots the player
  and ghost positions; afterwards it exposes `previous + alpha × (current − previous)` using
  `Engine::interpolationAlpha()`, so motion is smooth even though the model only steps at
  120 Hz. A tunnel wrap would make `current − previous` huge, so a jump wider than half the
  board is snapped instead of interpolated.
- **Rendering data**: `mazeData` and `itemData` are strings rebuilt from `Maze` (walls, door,
  tunnel, pellets, power pellets, fruit); the `ghosts` property is a `QVariantList` of maps
  (`x`, `y`, `id`, `eyes`, `edible`, `flashing`) so QML never touches a C++ type.
- **Events**: every `Event` drained from `Game` becomes a Qt signal — `levelStarted`,
  `pelletEaten`, `powerPelletEaten`, `fruitEaten`, `ghostEaten`, `ghostRespawned`,
  `playerDied`, `levelCompleted`, `gameOver` — which the QML side uses for popups and for
  sound effects. This is the mechanism `architecture.md` §6 describes.
- **Persistence**: the high score is read/written with `QSettings`. This is a *view* concern,
  which is why the model has no file I/O.
- **Pause**: `togglePause()` sets the engine time scale to `0` or `1` (only while `Running`).

### `app/Main.qml`

The window (640×760, black). A root `Column` takes focus and handles `Keys.onPressed`
(arrows/WASD → `game.requestDirection(...)`, `Space`/`Return` → `start()`, `P`/`Esc` →
`togglePause()`, `R` → `reset()`). Below a small HUD (score, high score, level name, and lives
shown as `life.svg` images) an `Item` holds a `Canvas` that repaints on `mazeChanged`/
`itemsChanged`/`actorsChanged`/`stateChanged`. The `Canvas` draws everything as vectors in the
classic arcade style ported from the original GTK project: blue outlined wall tiles, pure-white
pellets, a yellow wedge Pac-Man whose mouth animates from `playerMouth` (red while dying), and
dome-plus-body ghosts with white/black eyes — pure blue when frightened (white while flashing),
eyes-only when eaten. Overlays show `READY!`, `LEVEL CLEAR`, `GAME OVER`, `PAUSED` and
`NO LEVELS FOUND` from `stateName`. A `Connections` block turns the controller's signals into a
floating score popup with a `NumberAnimation`. Two `SoundEffect`s (`munch.wav`, `power.wav`)
are played from the `pelletEaten` and `powerPelletEaten` signals — this is the `app/`-side
audio that `architecture.md` §8 describes as the extension point for sound.

---

## The test suite

Described in detail in [`testing.md`](testing.md). In short:

| File | Covers |
| --- | --- |
| `tests/TestFramework.hpp` | registry, assertions, `PAC_TEST`/`CHECK*` macros |
| `tests/TestMain.cpp` | `registry()`, `runAll()`, `main()` |
| `tests/Fixtures.hpp` | `MAKE_LEVEL` and eight tiny hand-made maps |
| `tests/TestPrimitives.cpp` | `Direction`, `Vec2`, `Tile` |
| `tests/TestLevel.cpp` | parsing, options, every error path, duplicate markers, loading `level1.txt` and every file in `levels/` |
| `tests/TestLevelManager.cpp` | level list order and wrapping, file/directory loading, all-or-nothing failure |
| `tests/TestMaze.cpp` | geometry, pellets, doors, tunnels, BFS path finding |
| `tests/TestPlayer.cpp` | movement, turning, blocking, wrapping, counters |
| `tests/TestGhost.cpp` | targets per personality, tie-breaks, reversal, eyes, determinism |
| `tests/TestGame.cpp` | pellets, power pellets, collisions, lives, level flow, the level list, real level |
| `tests/TestEngine.cpp` | fixed stepping, clamping, time scale, reproducibility |
| `tests/TestReachability.cpp` | separate `pacman_tests_reach` binary: BFS that every pellet/fruit is reachable in every `levels/*.txt` |
| `tests/test_view.cpp` | optional `QtView` QTest (only with `PACMAN_BUILD_QT_TESTS=ON`): QML loads and a key event reaches the controller |