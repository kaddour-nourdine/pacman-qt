# Performance

The model is a fixed-step simulation: 120 steps per second, whatever the machine does. The only
question worth measuring is how much of the time a step is given (8.33 ms) the model takes — and
the only part of it that grows with the size of a level is the path search the eyes of a ghost
use, `Maze::stepToward`. Everything else in a step is a handful of arithmetic operations per
entity.

`tools/pacman_bench` measures both. It is **not** a CTest test: timings belong to a machine, not
to a repository, so it is run by hand.

```sh
cmake -S . -B build && cmake --build build -j
./build/tools/pacman_bench/pacman_bench                    # 20000 searches per row
./build/tools/pacman_bench/pacman_bench --iterations 200000
```

## The path search

Measured on a Ryzen 7 5800H, `RelWithDebInfo`, 20000 calls per row. "open" is a map with no walls
at all — the worst case for the search, because every cell is reachable.

| maze | cells | to the next tile | two random tiles | corner to corner |
| --- | --- | --- | --- | --- |
| `level1` | 441 | 144 ns | 3.8 µs | **6.5 µs** |
| open 10×10 | 100 | 58 ns | 1.4 µs | 2.9 µs |
| open 21×21 | 441 | 132 ns | 6.5 µs | 13.0 µs |
| open 50×50 | 2500 | 255 ns | 36.9 µs | 75.2 µs |
| open 100×100 | 10000 | 842 ns | 147.1 µs | 291.4 µs |

The three columns are three different questions:

- **to the next tile** is the shortest search there is — the goal is one step away, so the walk
  almost stops immediately. What is left is what the search costs *per call*, whatever it is
  asked: four vectors the size of the maze, allocated and filled, then released.
- **two random tiles** is the average case: a ghost that has just crossed a tile centre and has
  to pick the way to a target somewhere across the board.
- **corner to corner** visits every reachable cell of the maze; it is the ceiling.

Two things fall out of the table:

1. **The cost is the walk, not the bookkeeping.** `stepToward` allocates four vectors the size of
   the maze on every call (`parent`, `parentDirection`, `visited`, `queue` — `Maze.cpp:155-160`).
   That is 144 ns on `level1` and 842 ns on a 100×100 board — between **0.3 % and 2 %** of the
   corner search, measured. The rest is the breadth-first walk, at about **29 ns per cell it
   crosses** on an open board. The scaling is exactly linear in the cells, from the 100-cell
   board to the 10 000-cell one: 100× the board costs 100× the search, never 10 000×.
2. **A real maze is cheaper than its size suggests.** `level1` and the open 21×21 have the same
   441 cells, and `level1` is twice as fast (6.5 µs against 13.0 µs corner to corner): the walls
   cut the reachable area down, and the search stops at the first cell that reaches the goal.

## What a game actually asks for

A ghost decides once per tile centre it crosses. The fastest ghost (eyes, 16 tiles/s) crosses
16 tiles per simulated second, so a whole second of four ghosts coming home costs, worst case,
4 × 16 × 6.5 µs ≈ **415 µs — four hundredths of one per cent of the budget** (a simulated second
is one real second at 120 steps/s). And in a real game the eyes are usually close to home, so
the searches are the cheap ones, not the ceiling.

Measured end to end — the view's loop: 120 steps per second, a new input request every 37 steps,
four ghosts forced into eyes mode every 300 steps. Only the steps taken while the game is
*running* are counted, together with the wall-clock time they took: the death and
level-complete pauses are real time in which the model does almost nothing, and counting them
would make a scenario that dies often look like a fast one. The `restarts` column says how many
runs ended in a death.

| scenario | running steps/s | times the budget | restarts |
| --- | --- | --- | --- |
| `level1`, normal play | 2 906 000 | 24 213× | 30 |
| `level1`, four ghosts as eyes | 2 331 000 | 19 424× | 9 |
| open 50×50, four ghosts as eyes | 1 894 000 | 15 780× | 1 |

The eyes-heavy run is 20 % slower than normal play: that difference is the whole price of the
path search. On a board five and a half times the area of the shipped one (`level1` is 21×21,
the open 50×50 is 2500 cells), with every ghost pathing home at once and nowhere for the search
to stop early, the model still keeps four orders of magnitude of headroom.

## Consequences

- **No caching, no scratch buffers.** The todo asked for a measurement before adding anything;
  the measurement says a cache would buy nothing a player could see. It would also be a
  correctness risk: the result depends on the ghost's position, the door setting and the maze,
  and one stale entry is a ghost walking through a wall. The deterministic model is worth more
  than 6 µs.
- A single decision would only reach the 8.33 ms of one step on a maze of about **290 000
  cells** — a 540×540 board with no walls at all, some 650 times the area of `level1` and about
  26 times its edge. The level format and the parser would handle such a map; the view would
  give up long before the model did. Both halves of the cost are linear, so there is no size
  where the search suddenly falls off a cliff: the walk stays at ~29 ns per cell, and the four
  vectors (~0.08 ns per maze cell once they are past the fixed cost of four allocations) stay
  three orders of magnitude below it.
- If a level that big ever becomes real, the first thing to do is to **re-measure with this
  tool**, not to add a cache. The changes that would then be worth considering — reusing one
  set of bookkeeping buffers per `Maze`, or a parent array written once per search instead of
  allocated per call — are worth under 3 % today, which is inside the noise of this machine.
