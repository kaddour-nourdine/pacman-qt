# Coverage

The model in `core/` is the part of the game that has to be right: it decides where the ghosts
go, when the player dies and which points are scored. This page is the coverage baseline for it
— what is measured, what the numbers are today, and why the last few branches cannot be reached.

## Running the report

```sh
./tools/coverage.sh              # writes build-cov/coverage/index.html
./tools/coverage.sh build-cov2   # or into another directory
```

The script does the whole dance: it configures a `Debug` build with the compiler's coverage
counters (`--coverage -O0 -g`), builds the model and its tests, runs the whole CTest suite and
then asks `gcovr` for the report. It needs `gcovr` (`pip install gcovr`) and a GCC that supports
`--coverage`; with clang, use `llvm-cov` instead.

Two flags of `gcovr` matter for the numbers:

- `--exclude-unreachable-branches` drops the branches the compiler itself proved to be
  impossible. Without it the report counts every `switch` over an enum with a `default` case that
  can never be reached.
- `--exclude-throw-branches` drops the exception edges. On GCC they are the landing pads of the
  `try`/`catch` blocks and the cleanup paths of the constructors; they are counted as branches
  although no test can ever take them.

Both exclusions are about the *toolchain*, not about the code: with them the report answers
"which branch of this source line does no test take?", which is the question worth asking.

## The baseline

Measured on GCC 16.2 with `tools/coverage.sh`, over the whole `core/src`:

| | lines | branches | functions |
| --- | --- | --- | --- |
| `Engine.cpp` | 100 % (23/23) | 100 % (12/12) | 100 % (4/4) |
| `Entity.cpp` | 98 % (111/113) | 89 % (76/85) | 100 % (12/12) |
| `Game.cpp` | 97 % (311/319) | 93 % (212/227) | 100 % (29/29) |
| `Ghost.cpp` | 98 % (170/173) | 91 % (96/105) | 100 % (16/16) |
| `Level.cpp` | 99 % (217/219) | 98 % (257/263) | 100 % (8/8) |
| `LevelManager.cpp` | 95 % (64/67) | 75 % (68/91) | 100 % (8/8) |
| `Maze.cpp` | 97 % (140/143) | 94 % (126/134) | 100 % (14/14) |
| `Player.cpp` | 100 % (26/26) | 86 % (6/7) | 100 % (4/4) |
| **total** | **98.1 % (1062/1083)** | **92.3 % (853/924)** | **100 % (95/95)** |

Every function of the model is called by a test, and 21 lines are not executed. The 71 branches
that no test takes are listed below — each one with the reason why it cannot be reached. There is
no branch in the list that a test could still cover.

The Qt view (`app/`) is not part of this report. It is thin, it is covered by the `QtView` suite
and its rendering cannot be measured by a line counter anyway.

## The branches that cannot be reached

### A maze that is never null (10 branches)

`Entity.cpp:22,29,36,51,58,84,136` and `Ghost.cpp:75,76,200`

`Entity` and `Ghost` take a `Maze&` in their constructor and store its address in `maze_`.
That address is never null, so every `maze_ == nullptr` check guards against something that
cannot happen. They are leftovers from a version in which the maze was a pointer that could be
null; today they are pure noise, and the branch coverage counts each of them as a gap.

### The default case of a switch over a complete enum (8 branches)

`Entity.cpp:93,121`, `Game.cpp:8`, `Ghost.cpp:12,26,77,94,127`

Every one of these switches lists *all* the values of its enum, so the compiler still emits a
branch for "a value that is not in the list" — a value the type system cannot produce.

### Containers that are never empty (4 branches)

`Game.cpp:42,116,238,273`

The game always has four ghosts and the mode schedule always has at least one phase. The
`empty()` checks are the same kind of leftover as the null checks above.

### A frightened ghost whose timer has run out (5 branches)

`Game.cpp:261,263`

`applyModes` clears the fright mode of every ghost in the same step in which the fright timer
reaches zero. A ghost that is frightened therefore always has time left on its timer, and the
branch "frightened, but the timer is over" is never taken.

### The release schedule of Blinky (2 branches)

`Game.cpp:282`

The schedule releases Pinky, Inky and Clyde after 30, 60 and 90 pellets. Blinky is released
before the schedule ever runs, so the `case GhostId::Blinky` — which releases him anyway — is
never reached.

### A pellet that is not there (1 branch)

`Game.cpp:311`

The caller of `takePellet` checks the tile first; the "nothing was taken" case cannot happen
after that.

### The cleanup path of a constructor (2 branches)

`Game.cpp:66`

The exception edge of the delegating constructor `Game(Level)`. A `Level` cannot throw while it
is moved into the manager, so the cleanup code is never run.

### A frightened ghost that has to turn back (1 branch)

`Ghost.cpp:191`

When a frightened ghost arrives at a tile centre it turns around first
(`Ghost::onArriveAtCenter`). After that turn the way behind it is either open — in which case
it is one of the candidates and the list of options is not empty — or closed, in which case
`canMove(forbidden)` is false as well. The branch "no way but backwards" is therefore
unreachable.

### A mode schedule without parts (2 branches)

`Level.cpp:150,151`

`splitCommas` always returns at least one part, even for an empty value.

### A blank line before the first row of the map (1 branch)

`Level.cpp:204`

The first non-empty line of a level file always starts the map, so a blank line can only come
*after* a row — `mapRows` is never empty at that point.

### An empty map row (3 branches)

`Level.cpp:215,216`

A row is never empty: the parser skips empty lines, so every row of the map has at least one
character.

### A file system that fails twice (19 branches)

`LevelManager.cpp:52,53,57,58,67,68`

The directory is checked before it is listed, and the file that was just listed is inspected
right after. Both can only fail if the file system breaks *between* the two calls — a test
would have to pull the plug in the middle of the iteration.

### The "another thread is initializing it" path of a static (4 branches)

`LevelManager.cpp:98,107`

`levelFor` and `nameFor` return a function-local static when the manager is empty. The
compiler guards such a static with a variable that makes the initialization thread-safe; the
branch for "another thread is initializing this right now" needs a second thread. The program is
single-threaded by design.

### A tile outside the map (4 branches)

`Maze.cpp:40`

`rawTile` is private and is only called with a tile that lies on the map — either the tile
itself or the edge of a tunnel. The check that would catch a mistake cannot be triggered from
outside.

### A map without rows (2 branches)

`Maze.cpp:47,142`

A level always has rows, so `rows_ == 0` cannot happen. (`cols_ == 0` can: an empty `Level`
is a valid object, and `Maze` has to survive it.)

### The way back from the goal (2 branches)

`Maze.cpp:203,205`

The goal of the path search always gets a parent, so `previous < 0` cannot happen, and the loop
that walks back always ends with a `return`.

### A negative animation phase (1 branch)

`Player.cpp:39`

The distance travelled only grows, so the phase is never negative.

## Keeping the report honest

- A new branch in `core/` comes with a test that takes it. The report is a baseline, not a
  goal: it must not go down.
- A branch that *cannot* be reached does not need a test — but it should be listed above, so
  that the next person does not spend an afternoon looking for the test that is missing.
- The determinism digest (`docs/testing.md`) and the fuzzer (`tools/pacman_fuzz`) cover the
  model in ways a line counter cannot: they look for wrong answers, not for untaken lines.
