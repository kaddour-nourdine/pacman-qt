# Architecture

This document explains how the pieces fit together, why the project is split the way it is,
and what invariants the code relies on. Read it once, then start with
[`code-tour.md`](code-tour.md).

---

## 1. Why a split model / view

The final goal is one game that runs on a desktop and on Android. Those platforms differ in
almost everything except the game itself: window management, input, rendering, packaging.
So the game rules are isolated in a library that knows nothing about the platform, and a thin
view layer translates platform events into model calls and model state into pixels.

```
        ┌─────────────────────────────────────────────┐
        │  View (Qt Quick / QML)      — app/           │
        │                                             │
        │  input  ──────────────►  requestDirection() │
        │  clock  ──────────────►  Engine::update()   │
        │  draw   ◄──────────────  positions, score,  │
        │                          state, events      │
        └──────────────────────┬──────────────────────┘
                               │  public headers only
        ┌──────────────────────▼──────────────────────┐
        │  pacman_core (model)                        │
        │  Level · Maze · Entity · Player · Ghost ·    │
        │  Game · Engine                              │
        └─────────────────────────────────────────────┘
```

The view is `app/`: a `GameController` `QObject` owns the `Engine`, runs a 16 ms `QTimer`, and
is exposed to QML as a context property named `game`. It is the *only* file that includes both
Qt and `core/`, so the model stays portable. It does not re-implement rules: it reads the
player/ghost positions, interpolates them with `Engine::interpolationAlpha()` for smooth
drawing, forwards key presses as `requestDirection()` calls, and re-emits each drained `Event`
as a Qt signal (`pelletEaten`, `ghostEaten`, `playerDied`, …) for popups and future sound.

Consequences of the split, all visible in the code:

- `core/` has no `#include <Qt...>`, no graphics, no file I/O except loading level files, and
  no access to the system clock. You can unit-test it without a display (which is exactly what
  `tests/` does).
- The model exposes **data**, never handles: positions, directions, scores, enums, and a queue
  of `Event` structs. The view reads that data once per frame and draws it.
- Input is a *request*, not a command: the view calls `player.requestDirection(Direction::Up)`.
  The model decides whether and when the turn actually happens (see §4). This is what allows
  the authentic "queued turn" behaviour of the arcade game.
- `Game` hands out mutable references to the player and the ghosts
  (`Game.hpp:82`, `Game.hpp:84`). That is a deliberate seam for tests and for the future view,
  but production code should prefer the read-only accessors.

---

## 2. Layers

Each layer only knows the layers above it in this list:

| Layer | Files | Responsibility |
| --- | --- | --- |
| Value types | `Vec2.hpp`, `Direction.hpp`, `Tile.hpp`, `Random.hpp`, `Events.hpp` | Plain data and small helpers. No knowledge of the game. |
| Content | `Level.hpp/.cpp` | Turns a text file into validated, immutable level data (grid, spawn points, tuning values). |
| Campaign | `LevelManager.hpp/.cpp` | Owns the ordered list of levels: picks the level for a level number (wrapping at the end) and loads a directory of level files. |
| Board | `Maze.hpp/.cpp` | The *rules of the tiles*: what may be entered, where pellets are, how tunnels wrap, and how to find a path. |
| Movement | `Entity.hpp/.cpp` | The shared "walk in a straight line between tile centres" algorithm. |
| Characters | `Player.hpp/.cpp`, `Ghost.hpp/.cpp` | Who the characters are: spawn/reset, player stats, ghost AI targets. |
| Game rules | `Game.hpp/.cpp` | Order of updates, pellet scoring, collisions, lives, level flow, the scatter/chase schedule, event emission. |
| Time | `Engine.hpp/.cpp` | Converts variable real frame times into fixed simulation steps. |

The dependency graph is a chain: `Engine → Game → {Player, Ghost, Maze, LevelManager} → Entity
→ Maze → Level → {Tile, Direction, Vec2}`; `LevelManager` itself only depends on `Level`. There
are no cycles.

---

## 3. One frame of simulation

This is the single most important flow in the project. Suppose a 60 Hz window calls
`Engine::update(1.0/60.0)`:

```
Engine::update(0.0167)                         Engine.cpp:16
  ├─ scale by timeScale, clamp to maxFrameSeconds (0.25 s)
  ├─ accumulator += scaled
  └─ while accumulator >= 1/120:
        Game::step(1/120)                      Game.cpp:139
          └─ if state == Running: simulate()   Game.cpp:177
               1. updateFrightened(dt)   timers, then applyModes() for all ghosts
               2. movePlayer(dt)         player.update(dt) → Entity::update
               3. collectPellet()        remove pellet under the player, score, events
               4. moveGhosts(dt)         per ghost: eyes respawn? target? speed? update
               5. checkCollisions()      same tile? → eat ghost or kill player
               6. if allPelletsEaten() → finishLevel()
```

Three things are worth noticing:

- **The order is fixed** (`Game.cpp:177`). The player moves before the ghosts, so a collision
  is resolved using the positions at the end of the step. If you change the order you change
  the game feel, so treat it as a rule, not an accident.
- **Only `Running` simulates.** In `Ready`, `PlayerDied` and `LevelComplete` the call only
  advances the pause timer (`Game.cpp:149-175`). That is why the game does not move until you
  call `start()`.
- **`step` is the only way time passes.** The model never reads a clock. A test can therefore
  call `game.step(0.001f)` to advance a millisecond, or `engine.update(30.0)` to simulate half
  a minute.

### Two clocks, two jobs

| Type | Unit | Who uses it |
| --- | --- | --- |
| `float deltaSeconds` inside the model | seconds | the simulation |
| `double frameSeconds` at the boundary | seconds | the view, which knows real time |
| `int x`, `int y`, tile coordinates | tiles | everything spatial |

`Engine` is the only place the two meet. It also exposes `interpolationAlpha()`, the fraction
of a step already accumulated. A renderer can use it to draw entities *between* the previous
and the current simulation state and get perfectly smooth motion at any frame rate. The model
itself never interpolates — it only ever exposes exact simulation states.

---

## 4. The central invariant: decisions at tile centres

This is the design decision that makes the whole engine simple, and it is worth
understanding before reading `Entity.cpp`.

- A position is a continuous float pair. Tile `(x, y)` spans `[x, x+1) × [y, y+1)` and its
  centre is `(x + 0.5, y + 0.5)`.
- An entity travelling right from centre `(3.5, 1.5)` moves in small steps
  (`speed × dt` tiles per step). While it is *between* centres it simply keeps going.
- The moment it lands exactly on a centre, it gets to make one decision
  (`onArriveAtCenter()`, then `chooseDirection()`).
- A request from the view is remembered (`requestedDirection_`) and only honoured at a centre,
  and only if that tile can actually be entered. This gives the authentic behaviour where you
  may press Up slightly too early and Pac-Man still turns.
- If no direction is possible, `direction_` becomes `None` and the entity stands still
  (`isBlocked()`). It starts moving again as soon as a request becomes valid — without
  reversing (see `TestPlayer.restartsWhenBlocked`).

Consequences that the code depends on:

- `Entity::update` loops instead of jumping tile to tile (`Entity.cpp:135`), so a large
  `deltaSeconds` (a long frame, a test stepping 0.5 s, a fast speed) still yields correct
  turning behaviour.
- `snapToNextCenter()` (`Entity.cpp:120`) fixes floating-point drift by snapping the moving
  axis exactly onto the centre once it is reached.
- Speeds are tiles per second, so `dt × speed` is a distance in tiles and
  `distanceTravelled()` (used for the mouth animation) is a real distance.

---

## 5. Tile rules and the two boards

`Level` parses the file into an immutable grid. `Maze` then builds **two** grids:

- `base_` — the pristine level, pellets included. Never modified.
- `tiles_` — the live board. `takePellet(x, y)` clears a pellet here and decrements
  `pelletsRemaining_`. `resetPellets()` copies `base_` back.

This is why a level can restart without re-parsing the file (`Maze::resetPellets`,
`Maze.cpp:22`) and why tests can ask "how many pellets are left?" cheaply.

Who may enter what (`Maze::canEnter`, `Maze.cpp:78`):

| Tile | Player | Ghost |
| --- | --- | --- |
| `Empty`, `Pellet`, `PowerPellet` | yes | yes |
| `Wall`, outside the board | no | no |
| `Tunnel` | yes (wraps to the other side) | yes (wraps) |
| `Door` | **no** | yes |

The single flag `passDoors` encodes the last row. `Entity::canPassDoors()` returns `false`
for the player and `true` for ghosts (`Ghost.hpp:59`). In a level that uses doors (like
`levels/level2.txt`) the player is stopped at the doorway while the ghosts pass through;
`levels/level1.txt` has no door tiles, so its ghost area is open to everyone. The flag is also
what lets eaten ghosts (eyes) path back through a door.

Horizontal wrapping is data-driven: `Maze::wrapX` only wraps when the edge tile of that row
is a `Tunnel` (`Maze.cpp:46`). `level1.txt` has a tunnel row, so the maze is a torus in that
row only; every other row behaves like a normal rectangle. `tileAt` therefore never needs a
special case for the left/right edges.

---

## 6. Game states and events

`Game::state_` is a small state machine (`Game.hpp:15`):

```
             start()
   Ready ───────────────► Running ──────────────► PlayerDied ──┐
     ▲                       │  ▲                    │        │ pause 1.6 s
     │        all pellets    │  │  pause 2.0 s       │        │ (or GameOver
     │  beginLevel()         │  └──────────── LevelComplete ◄───┘  if lives == 0)
     └───────────────────────┴──────────────────────┘
```

`Game::step` (`Game.cpp:139`) is the only place that transitions between these states. When
the 2.0 s `LevelComplete` pause expires it first loads the next level from the `LevelManager`
(`++levelNumber_`, `loadLevel(levels_.levelFor(levelNumber_))`, `Game.cpp:171`) and only then
calls `beginLevel()`, so the view sees a fresh `LevelStarted` for level *n + 1* — or, with a
one-level list, for the same map again.

The view is not supposed to infer what happened by comparing two frames. The model reports
it: every interesting moment is pushed into an `Event` (`Events.hpp`) with the coordinates
and the points involved:

`LevelStarted`, `PelletEaten`, `PowerPelletEaten`, `FruitEaten`, `GhostEaten`, `GhostRespawned`,
`PlayerDied`, `LevelCompleted`, `GameOver`.

The view drains them once per frame with `drainEvents()` (`Game.cpp:426`) and turns them into
a sound, a score popup or a sprite flash. `swap` in `drainEvents` is what makes it cheap:
the queue is handed over empty and a fresh one is left behind. On `LevelStarted` the view
reads `levelNumber()` and `levelName()`; it never has to keep its own copy of "which level am
I on".

---

## 7. Determinism

A test that fails only sometimes is worse than no test. Three rules make the model
reproducible:

1. **Fixed timestep.** `Game::step` only ever receives the same constant
   (`Engine::kStepSeconds`), so no test can accidentally depend on frame rate.
2. **Seeded randomness.** The only nondeterminism a Pac-Man game needs is which way a
   frightened ghost turns. That comes from a tiny xorshift32 generator (`Random.hpp`),
   reachable through `Game::setRandomSeed`. No `rand()`, no `<random>` device, no time-based
   seeding. `TestGame.ghostsAreReproducibleForSameSeed` asserts two games with the same seed
   stay identical after 400 steps.
3. **No wall-clock time and no hidden global state.** Everything is a parameter or a member of
   the object under test. The one place that touches the filesystem is
   `LevelManager::loadDirectory`, and it sorts the file names before loading, so a directory
   of levels produces the same campaign order on every machine.

Floating point is exact enough for this: positions are `float`, the "am I on a centre?"
check uses a tolerance of `1e-4` (`Entity.cpp:10`) and positions are snapped when they land
(`Entity.cpp:120`).

---

## 8. Extension points

The design leaves a few obvious seams for work that has not been done yet:

| To add… | Where |
| --- | --- |
| More levels / a difficulty curve | drop `levelN.txt` into `levels/` and let `LevelManager::loadDirectory` pick it up; the knobs are `LevelConfig` |
| A level-select screen | `LevelManager::levels()` + `nameFor(n)`; the view only reads |
| Fruit, bonus items | `Tile::Fruit`/`FruitEaten` exist; timed spawn logic is still missing |
| Sound | `Event`s are enough; the Qt view now plays `SoundEffect`s in response to events |
| Sprite animation | `Player::animationPhase()` and `GhostMode` already expose what is needed |
| Different ghost personalities | `Ghost::chaseTarget` is one `switch` per ghost id |
| Save / high scores | the model has no file I/O; put it in the view or add an explicit `Storage` seam |
| Replay / netplay | possible because of determinism: log inputs, replay them |

Nothing in the current design needs to change to add any of these; that is the point of the
split.