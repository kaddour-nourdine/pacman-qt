# Testing

The project has 104 tests in 12 suites, all green, plus a separate reachability binary, a fuzz
run for the level parser and an optional Qt view suite:

```
$ ctest --test-dir build --output-on-failure            # default build
100% tests passed, 0 tests failed out of 13
$ ./build/tests/pacman_tests_reach
1/1 passed
$ ctest --test-dir build --output-on-failure            # with -DPACMAN_BUILD_QT_TESTS=ON
100% tests passed, 0 tests failed out of 14             # adds the QtView suite
```

There is no external test framework (no GoogleTest, no Catch2) — just `tests/TestFramework.hpp`
and `tests/TestMain.cpp`, about 130 lines together. It is worth understanding because you
will use it every time you change something.

---

## How the harness works

### Registration

`PAC_TEST(Suite, name)` expands to three things (`TestFramework.hpp:44`):

```cpp
#define PAC_TEST(suite, name)                                                   \
    static void suite##_##name##_body();                                        \
    static const pactest::Registrar suite##_##name##_registrar(                 \
        #suite, #name, &suite##_##name##_body);                                 \
    static void suite##_##name##_body()
```

1. a static function containing your test body,
2. a **static object** whose constructor (`Registrar::Registrar`, `TestMain.cpp:15`) appends
   `{suite, name, pointer}` to a global `registry()` vector,
3. the body itself.

Because the registrar is a global object with a constructor, it runs **before `main`**. That is
how the tests find each other without a macro that lists them anywhere. Two consequences worth
knowing: tests in one binary cannot be linked away by the linker (the registrars keep them
alive), and a test suite's execution order is the static-initialisation order, i.e. essentially
the link order of the translation units — never rely on it.

### Assertions

| Macro | What it checks |
| --- | --- |
| `CHECK(expr)` | `expr` is true |
| `CHECK_EQ(a, b)` | `a == b`, both cast to `long long` for printing (so enums and floats compare fine) |
| `CHECK_NEAR(a, b, eps)` | `|a - b| <= eps` for floats |

Each failing assertion calls `reportFailure`, which prints
`file:line: message` to **stderr** and increments a counter. **A failing `CHECK` does not stop
the test** — execution continues, so one mistake can produce several useful messages at once.

`CHECK_EQ` casts both operands to `long long`, which is convenient for enums
(`CHECK_EQ(game.state(), GameState::Ready)`) but means it is only appropriate for values that
are conceptually integral. Use `CHECK_NEAR` for floating-point comparisons and
`pac::nearlyEquals` (from `Vec2.hpp`) for vectors — `Vec2` has `operator==`, but exact
comparison of floats is almost never what you want.

### Running

```cpp
int main(int argc, char** argv) {
    const char* suite = argc > 1 ? argv[1] : nullptr;
    return pactest::runAll(suite);
}
```

- `./build/tests/pacman_tests` — everything, printing `ok`/`FAIL suite.name` and a
  `passed/total` line.
- `./build/tests/pacman_tests Ghost` — one suite.
- A non-zero exit code if any test failed. **Matching nothing is also a failure** (`runAll`
  returns 1 with `no tests matched suite 'X'`), which is what caught a mistyped suite name in
  the CTest list during development.

`tests/CMakeLists.txt` turns each suite name into a CTest test:

```cmake
foreach(suite Direction Vec2 Tile Level LevelManager Maze Player Ghost Game Engine ModeSchedule Determinism)
    add_test(NAME ${suite} COMMAND pacman_tests ${suite})
endforeach()
```

So `ctest` gives you per-suite pass/fail and can run a single suite with `ctest -R Ghost`. If
you invent a new suite name, add it here.

---

## Fixtures: small maps instead of the real level

`tests/Fixtures.hpp` provides `MAKE_LEVEL(...)`, which parses a map written inline and reports
a parse error as a test failure:

```cpp
Maze maze(pactest::makeLevel(pactest::houseMap(), __FILE__, __LINE__));
```

The eight maps exist because testing on a 21×21 maze makes assertions mushy. Each one isolates
one situation:

| Fixture | Shape | Exists to test |
| --- | --- | --- |
| `layoutMap()` | 11×7, player and all four ghosts spread out, one power pellet | geometry, spawn markers, pellet counting |
| `ringMap()` | 11×3 corridor with tunnels on both ends | tunnel wrapping (x becomes `−1` and comes back as `10`) |
| `houseMap()` | 11×8 with a sealed box `1234` behind a `-----` door | doors: the player is blocked, ghosts are not; BFS through the door |
| `junctionMap()` | 5×5 open room, player in a corner | ghost tie-breaking (only Up is legal there) |
| `gameMap()` | 13×7, house in the middle, one power pellet | full `Game` flows; 34 pellets, all reachable |
| `modeMap()` | `gameMap` plus a header: speeds `0.001`, `modeSchedule = 1,2`, `frightenedSeconds = 1.0` | mode schedule and fright timers without waiting real seconds |
| `firstRoomMap()` | 7×6, named `First Room`, 15 pellets, ghost markers in the open | the first entry of a two-level campaign |
| `secondRoomMap()` | 7×6, named `Second Room`, 12 pellets, a different layout | level progression: the game must notice that the maze changed |

`modeMap` is the most useful trick in the suite: it makes ghosts almost stationary, so a test
can run a whole scatter phase (`game.step(0.5f)`) in microseconds and assert on `globalMode()`.

---

## Suites

| Suite | Tests | What it pins down |
| --- | --- | --- |
| `Direction` | 4 | opposites, axis deltas, diagonal-to-`None` conversion |
| `Vec2` | 2 | arithmetic; `tileCenterOf` / `tileIndexOf` round trip |
| `Tile` | 2 | classification predicates; character ⇄ tile round trip |
| `Level` | 17 | geometry and markers, header options, defaults, **every error path** (ragged rows, no `P`, two `P`s, duplicate ghost marker, unknown option, bad number, invalid character, blank line in map, empty file, missing file), locale-independent number parsing, plus loading the real `level1.txt` and every file in `levels/` |
| `LevelManager` | 11 | empty list, 1-based picking with wrapping, `nameFor` labels, invalid level rejected, `clear`, loading `level1.txt`, and `loadDirectory`: sorted order, appending, all-or-nothing on a broken file, missing/empty directory |
| `Maze` | 10 | out-of-bounds as wall, pellets consumed once, level completion, `resetPellets`, doors per actor, tunnel wrapping, pellet-free tunnels, shortest path, path across a tunnel |
| `Player` | 12 | spawn, partial and whole tiles, exact one tile per second, turn applied at the next centre, ignoring a turn into a wall, stopping at a wall, restarting after a block, tunnel wrap, door blocked, counters, animation phase, reset |
| `Ghost` | 14 | spawn positions, point values, scatter corners, all four chase targets, tie-break preference, moving towards a target, reversal on fright, deterministic frightened choice, eyes pathing through the door, reset, `toString` |
| `Game` | 18 | ready-and-frozen, pellet collection and events, power pellet frightens ghosts, fright ending and schedule resumption, eating a ghost, eyes respawn, collision kills, game over after three lives, level completion and level 2, full reset, walking the `LevelManager` list (advance, wrap, lives carrying over, `reset` back to level 1, empty list ⇒ game over), seed reproducibility, the real level end to end, state names, soak test invariants, ghosts stay on walkable tiles after mid-tile reversal |
| `Engine` | 8 | fixed step size, partial-step accumulation, long-frame clamping, time scale, ignoring non-positive frames, no simulation while `Ready`, real-time driving, reproducibility with jittered frame times |
| `ModeSchedule` | 2 | default alternation, custom durations |
| `Determinism` | 4 | two identical replays agree, the digest matches the golden value, a different seed leads to a different game, the replay really exercises the game |

(`ModeSchedule`'s two tests physically live in `TestGame.cpp`; the suite name is what matters to
CTest.)

### The determinism replay

`tests/TestDeterminism.cpp` is the regression net for the whole model. It replays a recorded
input log — "at step *n*, ask for this direction" — against a level written inside the test file
and a fixed seed, and digests the trajectory:

- the **discrete state** after every step (tiles, directions, ghost modes, score, lives, pellet
  count). All of it is exact, so the digest never depends on how a compiler rounds a float;
- the **final positions**, quantized to 1/64 of a tile. An entity moves at least 3.5/120 of a
  tile per step, so a single step of drift is still visible, while a last-bit rounding
  difference is not.

Two things are worth knowing about it:

- The replay **restarts the game after a death**, the way the view does when the player presses
  a key. Without that, the game freezes in the `Ready` state, the ghosts reset to their spawn
  points and the digest stops depending on the seed — the replay would still be reproducible, but
  it would no longer pin anything about the ghost behaviour.
- The golden value (`kGoldenDigest` in the test file) is tied to the toolchain, because the
  positions are floats. Regenerate it **only** when a rule changes on purpose; a change that you
  did not ask for is a bug, and the test is telling you so.

A test that cannot fail is decoration — try it once: change the seed in `Ghost::chooseDirection`
and watch the `Determinism` suite go red.

---

## Techniques used, worth reusing

**Teleport instead of waiting.** `player.setPosition(tileCenterOf(10, 5))` drops the player onto
the power pellet; one `game.step(0.001f)` then eats it. Tests stay millisecond-fast and do not
depend on how fast the player would have walked there.

**Exact `step` sizes.** `game.step(0.001f)` is "advance one millisecond". Combined with a speed
of `0.001` tiles/second (`modeMap`) nothing visibly moves, so the test isolates one rule.

**Extract the event you care about.** `hasEvent(game.drainEvents(), EventType::GhostEaten)` in
`TestGame.cpp:13` checks a queue without caring about the others. `drainEvents()` empties the
queue, so tests stay independent — `CHECK(game.drainEvents().empty())` verifies that.

**Reproducibility instead of exact expected values.** `TestEngine.isReproducible` runs two
games with jittered frame times and the same seed, and asserts they end up identical. That
catches "somewhere a hidden dependency on frame rate crept in", which no hand-written expected
value would.

**Test the real asset too.** `TestGame.playsRealLevelEndToEnd` loads `levels/level1.txt`,
checks 178 pellets and Pinky's spawn, runs 600 frames through `Engine`, and asserts the score
went up, pellets were eaten and a ghost left the house. A hand-designed map can pass every unit
test while the actual level file is broken; this test is the safety net. `PACMAN_LEVELS_DIR`
is a compile definition so the test can find the file wherever the build directory is. The Qt
suite's `ViewTest.testBundledLevelsAreShipped` covers the other copy of the campaign — the one
embedded as Qt resources — by materializing it into a test-mode directory and loading it with
`LevelManager`.

---

## Writing a new test

1. Pick or add a fixture map in `tests/Fixtures.hpp` if you need a specific board shape.
2. Put the test next to its subject: `tests/TestGhost.cpp` for ghost behaviour, and so on.
3. Use the smallest meaningful `step` and drive time explicitly.
4. If you invented a new suite name, add it to the `foreach` in `tests/CMakeLists.txt`.
5. Build and run:

```sh
cmake --build build -j && ./build/tests/pacman_tests Ghost && ctest --test-dir build
```

A test is a good test if it fails when you break the feature it covers. Try that once: comment
out a line in `Ghost::chaseTarget`, watch the suite go red, then put it back. A test that cannot
fail is decoration.

---

## What is not covered yet

- **The Qt view**: a headless `QTest` (`tests/test_view.cpp`) is wired via
  `PACMAN_BUILD_QT_TESTS=ON` and registered as the `QtView` CTest test. It links the `pacman_view`
  library, loads `Main.qml` with `QQmlApplicationEngine`, sends a Right-arrow key event to the
  window and asserts the controller received it (the turn is queued, so it checks the player's
  *requested* direction). It runs under `QT_QPA_PLATFORM=offscreen` (set on the test by CTest)
  and passes. Still untested: the rendered output, input devices and timing on a real display.
  - **Note**: Qt suppresses `qWarning` output when stderr is not a terminal. Prefix headless
    runs with `QT_FORCE_STDERR_LOGGING=1` to see QML warnings.
- **Reachability**: `tests/TestReachability.cpp` BFS-checks that every pellet (and fruit, if
  present) is reachable, for every level. It builds a separate `pacman_tests_reach` binary and is
  **not yet registered** as a CTest test (add it to `tests/CMakeLists.txt`).
- **Property-based / fuzz testing**: the parser *is* fuzzed now — `tools/pacman_fuzz` generates
  random headers and maps, mutates the levels that ship with the game and checks that `parse`
  either produces a level that agrees with the input or fails with a message that says why. It
  runs as the `LevelFuzz` CTest test (20 000 cases, fixed seed). The model itself is not fuzzed
  yet: the gameplay rules are hand-tested, not generated.