# Gameplay rules as implemented

This document lists the rules the code actually enforces, where each one lives, and — at the
end — the places where this implementation knowingly differs from the 1980 arcade original.
If you ever wonder "why does the ghost do that?", this is the file to check.

---

## 1. The board

- The board is a grid of tiles. Tile `(x, y)` covers `[x, x+1) × [y, y+1)`; its centre is
  `(x + 0.5, y + 0.5)` (`Vec2.hpp:60`, `tileCenterOf`).
- `Maze::tileAt` treats anything outside the grid as `Wall` (`Maze.cpp:57`).
- **Tunnels:** `Maze::wrapX` wraps a position from one side to the other *only* in a row whose
  edge tile is `Tunnel` (`Maze.cpp:40`). `Maze::wrapPosition` does the same for a float
  position (`Maze.cpp:99`) and is applied after every movement step, so an entity never leaves
  the board. In `level1.txt` that is row `y = 10`.
- **Doors** (`-`) separate the ghost house from the maze. A ghost may pass, the player may not
  (`Maze::canEnter`, `Maze.cpp:72`, with the flag coming from `Entity::canPassDoors`).
- Outside the board counts as a wall, so nobody can ever walk off the edge.

## 2. Movement

Implemented once in `Entity::update` (`Entity.cpp:123`) and shared by the player and ghosts.

- Speed is in **tiles per second**. Per step the entity wants to travel `speed × dt` tiles.
- Movement only happens in straight lines between tile centres.
- **Turning:** when an entity reaches a tile centre it may pick a new direction
  (`Entity::chooseDirection`, `Entity.cpp:68`). Priority: the requested direction if it is
  legal, otherwise keep the current direction, otherwise stop.
- **Queued turns:** the view stores intent with `requestDirection()`. It is honoured at the
  next tile centre only, and only if that tile can be entered. Pressing Up against a wall does
  nothing and does not clear the request, so the player keeps moving and turns at the next
  opportunity.
- **Blocked:** with no legal direction, `direction_` becomes `None` and the entity stops dead
  (`isBlocked()`, `Entity.hpp:37`). It does **not** reverse. (The arcade ghosts reverse; this
  engine deliberately does not, which keeps the player from being pushed around.)
- **Ghosts reverse** — but only in two cases: when `setMode(Frightened)` or `setMode(Eyes)` is
  called (`Ghost.cpp:60`), and a frightened ghost reverses at every tile centre
  (`Ghost::onArriveAtCenter`, `Ghost.cpp:152`).
- Tunnel wrapping is applied automatically by `advance()` (`Entity.cpp:78`), so a ghost can be
  travelling left off the left edge and reappear on the right.
- Floating point drift is corrected by `snapToNextCenter()` (`Entity.cpp:108`) whenever an
  entity lands on a centre.

## 3. The player

`Player` (`Player.hpp`) is an `Entity` plus two counters and one cosmetic helper.

- Spawns on the `P` marker of the level, facing left (`Player::Player`, `Player.cpp:7`).
- `reset()` restores position, direction, speed and zeroes the counters (`Player.cpp:15`). It
  is called on level start and after every death.
- `notePelletEaten()` counts pellets and power pellets (`Player.cpp:26`). Counting lives in the
  player because it is per-life/per-level statistics the view may want to show.
- `animationPhase()` returns a value that cycles `0 → 1` twice per tile travelled
  (`Player.cpp:36`). It exists so a renderer can open and close Pac-Man's mouth without
  duplicating distance logic. The model never draws anything.

## 4. Ghosts

`Ghost` (`Ghost.hpp`) is an `Entity` plus an identity, a mode and a target tile. Four
instances exist, `GhostId::Blinky/Pinky/Inky/Clyde` (`Ghost.hpp:8`), created by `Game` from the
level's spawn markers.

### Modes

| Mode | Behaviour | Speed |
| --- | --- | --- |
| `Scatter` | head for the ghost's own corner | `ghostSpeed` |
| `Chase` | head for a target derived from the player | `ghostSpeed` |
| `Frightened` | random legal direction, reversing at every centre | `frightenedSpeed` |
| `Eyes` | shortest path back to the spawn tile, doors allowed | `eyesSpeed` |

Speed selection is `Game::speedFor` (`Game.cpp:221`), with one special case: a ghost standing
on a tunnel tile uses `tunnelSpeed` (ghosts visibly slow down in tunnels, like the arcade).

### Targets

`Ghost::updateTarget` (`Ghost.cpp:123`) sets `target_` from the mode; the mode is set by
`Game::applyModes` (`Game.cpp:202`), never by the ghost itself.

- **Scatter corners** (`Ghost::scatterCorner`, `Ghost.cpp:74`), in maze coordinates:
  Blinky top-right, Pinky top-left, Inky bottom-right, Clyde bottom-left.
- **Chase targets** (`Ghost::chaseTarget`, `Ghost.cpp:90`):
  - *Blinky* — the player's tile.
  - *Pinky* — four tiles in front of the player (the direction the player is facing).
  - *Inky* — take the tile two in front of the player, draw the vector from Blinky to it, and
    double it. (This is the famous "flanking" target.)
  - *Clyde* — the player's tile when further than 8 tiles away, otherwise his own scatter
    corner, which makes him timid up close.
- **Frightened** — the target is ignored; direction is random.
- **Eyes** — the spawn tile, reached by BFS path finding (`Maze::stepToward`,
  `Maze.cpp:129`) with doors passable.

### Choosing a direction at a centre

`Ghost::chooseDirection` (`Ghost.cpp:199`):

1. Never reverse (unless nothing else is possible).
2. Normal modes: among the legal candidates, pick the one whose neighbour tile centre is
   **closest to the target**, measured with squared distance. Ties keep the first candidate in
   the preference order **Up, Left, Down, Right** (`kGhostPreference`, `Direction.hpp:15`),
   because the comparison is strict `<`.
3. Frightened: pick uniformly at random from the legal candidates (`Random::range`, seeded —
   see `docs/architecture.md` §7).
4. Eyes: ask `Maze::stepToward` for the first step of a shortest path.
5. If nothing is legal, allow the reverse direction as a last resort.

## 5. Scatter / chase schedule

`ModeSchedule` (`Game.hpp:31`) turns "how long since the last fright" into "scatter or chase".

- Default durations: `7, 20, 7, 20, 5, 20, 5, 20` seconds, alternating
  scatter → chase → … (`Game.cpp:27`).
- `modeAt(seconds)` walks the list and returns `Scatter` for even indices, `Chase` for odd
  ones (`Game.cpp:41`). After the list runs out it stays in the last phase forever.
- A level file can override it with `modeSchedule = 1,2` (see
  [`level-format.md`](level-format.md)), which is how the tests run a whole phase in half a
  second.
- `Game::updateFrightened` (`Game.cpp:190`) only advances `scheduleSeconds_` while no power
  pellet is active, and eating a power pellet **resets the schedule clock to zero** and forces
  `Scatter` (`Game.cpp:202`). This reproduces the arcade behaviour where a power pellet
  interrupts the wave pattern.

## 6. Power pellets, frightened mode and eating ghosts

- Eating a power pellet (`Game::collectPellet`, `Game.cpp:302`): +50 points, sets
  `frightenedSecondsLeft_` to `frightenedSeconds`, resets the schedule, forces scatter, and puts
  every ghost that is not already eyes into `Frightened`.
- A ghost is *edible* exactly while it is frightened (`Ghost::isEdible`, `Ghost.hpp:35`).
- Eating a ghost: +200 / 400 / 800 / 1600 points by ghost id (`Ghost::pointsValue`,
  `Ghost.cpp:146`, table `kPointValues` at `Ghost.cpp:7`). The pellet points of that power
  pellet are *not* added to the ghost points in this implementation — the arcade adds them.
- `Ghost::eat()` (`Ghost.cpp:150`) switches the ghost to `Eyes`, clears any queued turn and
  makes it reverse.
- While `frightenedSecondsLeft_ > 0`, `applyModes` skips ghosts that are already eyes, so they
  keep heading home even after the fright expires. Once the eyes reach their spawn tile,
  `Game::moveGhosts` resets them (`Game.cpp:235`) and emits `GhostRespawned`.

## 7. Scoring and lives

| Event | Points |
| --- | --- |
| Pellet | 10 (`Game::kPelletPoints`) |
| Power pellet | 50 (`Game::kPowerPelletPoints`) |
| Blinky / Pinky / Inky / Clyde eaten | 200 / 400 / 800 / 1600 |

- Score lives in `Game::score_`, is only ever increased, and is reset by `Game::reset()`.
- Lives come from `startingLives` (default 3). A collision with a non-frightened ghost in the
  same tile costs one life (`Game::killPlayer`, `Game.cpp:366`).
- Collisions are **tile-based**: player and ghost collide when their tile indices are equal
  (`Game::checkCollisions`, `Game.cpp:337`). Two ghosts passing through each other in opposite
  directions do not interact, and so does a player who is *between* two tiles next to a ghost.
  A pixel-accurate version would need an overlap test instead — a good exercise.

## 8. Level flow

- `Game::beginLevel` (`Game.cpp:105`) restores all pellets, resets the player and the ghosts,
  clears timers, sets `state_ = Ready` and emits `LevelStarted`. With an empty level list it
  reports `GameOver` instead: there is nothing to play.
- `Game::start()` (`Game.cpp:133`) moves `Ready → Running`. This is the "get ready" pause of
  the arcade; nothing simulates before it.
- `allPelletsEaten()` (`Maze::allPelletsEaten`, needs at least one pellet in the level) ends the
  level: `LevelComplete` for 2 seconds, then `levelNumber_++`, `loadLevel` of
  `levels_.levelFor(levelNumber_)` and `beginLevel()` again (`Game.cpp:171`).
- `levelFor` wraps past the end of the list, so with one level (today's `levels/level1.txt`)
  the same maze replays forever, while a full list makes the campaign loop like the arcade.
- Lives and score carry over between levels; only the constructors and `reset()` apply
  `startingLives` again. The next level's speeds, frightened seconds and `modeSchedule` take
  effect as soon as it is loaded, because `loadLevel` rebuilds the schedule from its header.
- Death flow: `PlayerDied` for `kDeathPauseSeconds` (1.6 s), then either reset the level or,
  if `lives_ == 0`, `GameOver` (`Game.cpp:151-168`).
- The view reads the label with `game.levelName()` and the number with `game.levelNumber()` —
  it never tracks the progression itself.

---

## Differences from the arcade original

Worth knowing so you do not "fix" behaviour that is intentional here, and so you know what
fidelity work remains:

1. **Ghost house release timers** are simplified (pellet-based thresholds: Blinky 0, Pinky 30, Inky 60, Clyde 90). Full arcade behavior with LeavingHouse pathing is not fully modeled; ghosts use normal AI once released.
2. **Any ghost may wander into the house.** Door tiles are walkable for ghosts, and target
   selection does not forbid the house interior. In the arcade only the doorway tiles are
   usable during play. Eyes deliberately do the opposite (they need the door).
3. **No fruit / bonus items, no bonus life thresholds**, no inter-level cut-scenes.
4. **Frightened warning flash** is available via predicates (`Game::frightenedFlash()`, `Ghost::isFrightenedFlashing()`); ghost behavior/visuals can use these.
5. **Corner turn restriction** prevents upward turns in the tiles above the ghost house door for level1 (ty==5, tx in {8..11}); this is hardcoded for now and not data-driven.
6. **Frightened time does not stop while the player is dead** or while a level transition
   pause is running.
7. **The eaten-ghost score does not include the current power pellet bonus**, and the ghost
   eaten counter within one power pellet is not reset explicitly (the fright timer handles it).
8. **No difficulty curve yet**: `startingLives`, speeds, frightened seconds and the schedule
   are per level *file*, and `Game` now walks a `LevelManager` list of them — but `levels/`
   still contains exactly one file, so every level is currently that same maze.
9. **Extra lives** at score thresholds are not implemented.
10. **Collisions are per tile**, not per pixel (see §7).