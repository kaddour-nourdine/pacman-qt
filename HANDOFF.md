# HANDOFF — session notes

Written at the end of the first working session so the next one can start without
re-deriving anything. `todo.md` holds the plan; `docs/` holds the explanations. This file
holds only the state and the constraints.

## Verified state

| | |
| --- | --- |
| Working directory | `/home/nourdine/Development/pacman-qt` |
| Version control | git (branch master) |
| Build | clean, no warnings with `-Wall -Wextra -Wpedantic` |
| Tests | 81 tests, 10 CTest suites, **10/10 passing** |
| Toolchain used | CMake 4.3.0, g++ (GCC) 16.2.1, Linux, 16 cores |
| Qt | **not installed** (`qmake6` absent; only a Qt runtime path exists) — the Qt view cannot be started yet |
| Last verified commands | `cmake -S . -B build && cmake --build build -j16 && ctest --test-dir build` |

`build/` exists and is current, so a rebuild is incremental.

## Constraints agreed in this session

- Work **only inside this directory**. Do not probe or modify anything outside it, and do not
  install system packages without asking.
- `core/` must stay free of Qt, graphics, platform APIs, wall-clock time and file I/O other
  than reading a level file. This is what makes the model reusable on Android.
- Game rules live in `Game`/`Maze`/`Ghost`; the view may only read state, send input requests
  and react to `Event`s.
- The model is deterministic: fixed 1/120 s steps, seeded xorshift RNG, no `rand()`.

## What exists

- `core/` — the whole simulation (see `docs/code-tour.md` for the file-by-file tour).
- `levels/level1.txt` — 21×21, 170 pellets, validated by hand and by
  `TestGame.playsRealLevelEndToEnd`.
- `tests/` — custom harness, no external dependency.
- `README.md`, `docs/{architecture,code-tour,gameplay-rules,level-format,testing}.md`,
  `todo.md`.
- **Not** written: any view/renderer, any Qt or Android code, any level list.

## Open items carried over

1. **Duplicate ghost markers are silently accepted** — `Level::parse` (`core/src/Level.cpp:231`)
   overwrites an earlier `1`–`4` with a later one instead of failing. The only real defect found
   so far; documented in `docs/level-format.md`, fix listed first in `todo.md` Phase 1.
2. **Git history**: repo initialized; Phase 0/1 work committed. Branch: master.
3. `Game::checkCollisions` uses tile comparison, not sprite overlap — currently intentional and
   documented (`docs/gameplay-rules.md` §7), revisit only as a deliberate fidelity decision.
4. Ghost house release timers (pellet-based), frightened flash predicates, and corner turn restriction (level1 house area) are partially/explicitly implemented; see `docs/gameplay-rules.md` "Differences" for current state.

## Suggested first action tomorrow

```sh
cd /home/nourdine/Development/pacman-qt
git status && git log --oneline -5
cmake -S . -B build && cmake --build build -j16 && ctest --test-dir build
```

Phase 1 is largely complete. Continue from `todo.md` Phase 2 (LevelManager/level list) or Phase 3 (`tools/pacman_text`) if you want to watch the game run.