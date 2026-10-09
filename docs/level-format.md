# Level file format

Levels are plain text. They live in `levels/`, are read by `Level::loadFile`
(`core/src/Level.cpp:294`) and validated by `Level::parse` (`core/src/Level.cpp:154`). There is
no binary format and no asset pipeline: edit the file, rebuild nothing, play.

```
levels/level1.txt
```

A file has two parts, in this order:

```
key = value          ← optional header, one per line
key = value
                      ← one blank line ends the header
#############         ← the map: rows of characters, all the same width
#############
```

---

## Map characters

| Char | Tile | Meaning |
| --- | --- | --- |
| `#` | `Wall` | Wall. Nobody enters. |
| `.` | `Pellet` | A pellet: +10 points, one per level completion. |
| `o` | `PowerPellet` | A power pellet: +50, frightens every ghost. |
| ` ` | `Empty` | Open corridor with nothing in it. |
| `-` | `Door` | The ghost house door. Ghosts may pass, the player may not. |
| `_` | `Tunnel` | Wraps to the opposite edge. Never contains a pellet. |
| `P` | *marker* | Where the player starts (exactly one per level). The tile itself becomes `Empty`. |
| `1` | *marker* | Blinky start (tile becomes `Empty`). |
| `2` | *marker* | Pinky start. |
| `3` | *marker* | Inky start. |
| `4` | *marker* | Clyde start. |

The conversion functions are `charToTile`, `tileToChar`, `isMapChar` and `isGhostMarker` in
`core/include/pacman/Tile.hpp`. Anything else in the map is a hard error, so typos cannot
silently become walls.

Trailing spaces matter: a row of the map is exactly as wide as the first row, and the space
character is meaningful. Keep the file's trailing whitespace intact.

---

## Header options

Every key is optional; the defaults come from `LevelConfig` (`core/include/pacman/Level.hpp:12`).

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `name` | text | `untitled` | The level's label: `Game::levelName()` / `LevelManager::nameFor(n)`. |
| `playerSpeed` | float > 0 | `6.0` | Player tiles per second. |
| `ghostSpeed` | float > 0 | `5.0` | Ghost tiles per second while scattering/chasing. |
| `frightenedSpeed` | float > 0 | `3.5` | Ghost speed while frightened. |
| `tunnelSpeed` | float > 0 | `4.0` | Ghost speed while standing on a tunnel tile. |
| `eyesSpeed` | float > 0 | `16.0` | Speed of an eaten ghost returning home. |
| `frightenedSeconds` | float > 0 | `6.0` | How long a power pellet frightens the ghosts. |
| `startingLives` | int > 0 | `3` | Lives at the start of the game. |
| `modeSchedule` | comma list of floats | `7,20,7,20,5,20,5,20` | Scatter/chase phases in seconds, alternating scatter first. |

Notes:

- Values are parsed strictly (`Level.cpp:68`, `applyHeaderEntry`): the whole value must be a
  number, trailing junk is rejected, and speeds/durations must be positive.
- Numbers always use a `.` decimal separator, regardless of the user's locale. Parsing uses
  `std::from_chars` rather than `strtof`, because the latter would read `6.0` as "six" under a
  locale that uses a comma (which is what a Qt app installs on start-up). Always write `6.0`,
  never `6,0`.
- An unknown key is an error (`"unknown level option"`), which catches misspelled keys instead
  of ignoring them.
- `modeSchedule = 1,2` is how the tests compress the wave pattern into a couple of seconds.

---

## What the parser checks

`Level::parse` rejects a file (returning `false` and filling in an error string) when:

- the file has no map body, or the first map row is empty;
- two map rows have different widths (`"all map rows must have the same width"`);
- a map character is not in the table above;
- a blank line appears *inside* the map (`"blank line inside the map body"`); a single blank
  line between header and map is fine, and so is a trailing one at end of file;
- there is no `P`, or more than one `P`;
- a ghost marker `1`–`4` appears twice (`"map has more than one ghost marker '2'"`);
- no walkable tile is left for a ghost that has no marker.

Keep exactly one of each of `1`–`4`, or omit a marker entirely and let the parser place that
ghost on the first free tile. A typo such as `2324` now fails loudly instead of silently moving
Pinky.

Errors are always produced, never thrown, and always readable:

```cpp
Level level;
std::string error;
if (!Level::loadFile("levels/level1.txt", level, &error)) {
    std::fprintf(stderr, "bad level: %s\n", error.c_str());
}
```

### Ghosts without a marker

If a level omits `1`–`4`, `Level::parse` places the missing ghosts on the first walkable tile
that is not the player's start, scanning row by row (`Level.cpp:267`) and flagging
`GhostSpawn::fromMap = false` so the caller can tell the difference. Small test levels
therefore only need a `P`.

---

## `levels/level1.txt` as an example

```
name = Level 1
playerSpeed = 6.0
...
modeSchedule = 7,20,7,20,5,20,5,20

#####################
#.........P.........#
#.##.#.###.###.#.##.#
#o#......#.#......#o#
#.#.#.#.##.##.#.#.#.#
#.#.....##1##.....#.#
#.#.######-######.#.#
#.#.#### 2 4 ####.#.#
#.#.####  3  ####.#.#
#.#.#############.#.#
_                   _
#.##.#.###.###.#.##.#
#o#......#.#......#o#
#.#.#.#.##.##.#.#.#.#
#.#...#.##.##.#...#.#
#.###.#.##.##.#.###.#
#...................#
#.####.###.###.####.#
#.####.###.###.####.#
#...................#
#####################
```

Facts worth knowing (all asserted by tests):

| | |
| --- | --- |
| Size | 21 × 21 tiles |
| Player start | `(10, 1)`, facing left |
| Ghost house door | `(10, 6)` |
| Blinky `1` | `(10, 5)` — above the door, with an open tile above it |
| Pinky `2` | `(9, 7)` — inside the house |
| Inky `3` | `(10, 8)` — inside the house |
| Clyde `4` | `(11, 7)` — inside the house |
| Power pellets `o` | `(1, 3)`, `(19, 3)`, `(1, 12)`, `(19, 12)` |
| Tunnels | `(0, 10)` and `(20, 10)` |
| Pellets | 159 total (155 `.` + 4 `o`) |

The maze is left-right mirror symmetric apart from the one ghost at `(10, 5)`, which is what
lets the ghost AI be predictable. Three invariants were checked before the level was accepted:

1. Every pellet is reachable by the player (BFS with doors closed).
2. Every pellet is reachable by a ghost (BFS with doors open).
3. The house interior is not reachable by the player.

`TestGame.playsRealLevelEndToEnd` re-checks (1)–(3) indirectly on every test run by loading the
file and simulating 600 frames.

---

## Designing your own level

1. Copy `levels/level1.txt` to `levels/mylevel.txt` and edit the map.
2. Keep every row exactly as wide as the others; use spaces for empty corridor.
3. Put exactly one `P`. Put the four ghost markers inside the house, behind a `-` door.
4. Make the perimeter solid (`#`) except where you want a `_` tunnel — wrapping only happens
   at tunnel tiles, so a gap without `_` is simply a wall to the outside world.
5. Verify it:

```sh
./build/tests/pacman_tests Level        # parser tests, every file in levels/
```

Everything in `levels/` is loaded by `TestLevel.loadsAllLevelFilesInLevelsDirectory` on every
`ctest` run, so a broken file fails the build even if no test names it.
`LevelManager::loadDirectory` reads the same directory for the game itself: it loads every
`*.txt` **sorted by file name**, which is the campaign order — so name the files `level01.txt`, `level02.txt`, …
rather than `level1.txt` … `level10.txt`, otherwise `level10` sorts before `level2`.

There is no dedicated "validate my file" tool yet. Until there is one, the quickest check is
to add a line to `tests/TestLevel.cpp` calling
`Level::loadFile("levels/mylevel.txt", level, &error)` and run the `Level` suite — it prints
the parser's error message if anything is wrong.

Handy sizes: the map must be odd in both dimensions if you want a single centre column
(21×21 is the arcade size and works well with the ghost AI's 4-tile and 8-tile target
distances).