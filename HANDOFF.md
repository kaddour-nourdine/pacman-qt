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
| Tests | 99 tests, 11 CTest suites, **11/11 passing** (plus `pacman_tests_reach`) |
| Toolchain used | CMake 4.3.0, g++ (GCC) 16.2.1, Linux, 16 cores |
| Qt | **6.11 installed** (`qt6-qtbase-devel`, `qt6-qtdeclarative-devel`); QtQuick/QtQuick.Window/Controls runtime modules present |
| View | `app/` builds with `-DPACMAN_BUILD_APP=ON`; runs under `QT_QPA_PLATFORM=offscreen` |
| Last verified commands | `cmake -S . -B build -DPACMAN_BUILD_APP=ON && cmake --build build -j16 && ctest --test-dir build` |

`build/` exists and is current, so a rebuild is incremental.

## Constraints agreed in this session

- Work **only inside this directory**. Do not probe or modify anything outside it, and do not
  install system packages without asking.
- `core/` must stay free of Qt, graphics, platform APIs, wall-clock time and file I/O other
  than reading level files (`Level::loadFile`, `LevelManager::loadDirectory`). This is what
  makes the model reusable on Android.
- Game rules live in `Game`/`Maze`/`Ghost`; the view may only read state, send input requests
  and react to `Event`s.
- The model is deterministic: fixed 1/120 s steps, seeded xorshift RNG, no `rand()`.

## What exists

- `core/` — the whole simulation (see `docs/code-tour.md` for the file-by-file tour).
- `levels/level1.txt`, `levels/level2.txt` — two playable levels; `level2.txt` has a shorter
  scatter schedule, faster ghosts and fewer frightened seconds.
- `core/LevelManager` — owns the campaign: `add`/`loadFile`/`loadDirectory`, 1-based
  `levelFor` (wraps), `nameFor` for the view. `Game` takes it by value, advances on
  `LevelComplete`, reports `GameOver` on an empty list.
- `tools/pacman_text` — ASCII terminal renderer with arrow/WASD input; `tools/ai_demo` — a
  scripted demo. Both drive `Engine` and only read `Game`.
- `app/` — the Qt Quick view (`PACMAN_BUILD_APP`, default `OFF`): `GameController` is the only
  bridge to the model (Q_PROPERTY + invokable slots + per-`Event` signals); `Main.qml` draws the
  maze/actors on a `Canvas`, shows the HUD/overlays, and handles keys.
- `tests/` — custom harness, no external dependency; `tests/TestLevelManager.cpp` covers the
  loader (temp directories, all-or-nothing, sorted directory scan) and `tests/TestReachability.cpp`
  is a separate binary that BFS-checks every level.
- `README.md`, `docs/{architecture,code-tour,gameplay-rules,level-format,testing}.md`, `todo.md`.

## Open items carried over

1. **Qt app is unverified on a real display** — only checked headless (`offscreen`). Confirm the
   window, keyboard focus and `Canvas` rendering on a desktop session.
2. **Locale-sensitive parsing is fixed** — `Level.cpp` now uses `std::from_chars` (was `strtof`,
   which broke under Qt because `QGuiApplication` installs `fr_FR` and its comma decimal
   separator). A regression test (`Level.parsesIndependentlyOfNumericLocale`) covers it.
3. `Game::checkCollisions` uses tile comparison, not sprite overlap — currently intentional and
   documented (`docs/gameplay-rules.md` §7), revisit only as a deliberate fidelity decision.
4. Ghost house release timers (pellet-based), frightened flash predicates and the corner-turn
   restriction are partially/explicitly implemented; see `docs/gameplay-rules.md` "Differences".
5. Known quirk: `Ghost::forbidUpwardTurn()` forbids Up entirely at the spawn tile row, so Blinky
   may bounce back into the house in `level1.txt` — verified by inspection, not yet fixed.
6. Phase 4 remaining work: audio (`SoundPlayer` on `Event`s), a `QTest` headless assertion that a
   key event reaches `player().direction()`, and sprite rendering.

## Suggested first action next session

```sh
cd /home/nourdine/Development/pacman-qt
git status && git log --oneline -8
cmake -S . -B build -DPACMAN_BUILD_APP=ON && cmake --build build -j16 && ctest --test-dir build
QT_QPA_PLATFORM=offscreen ./build/app/pacman_app    # quick smoke run
```

Phases 1–3 are complete and Phase 4 is under way. Continue from the remaining Phase 4 items in
`todo.md` (audio, headless `QTest`, sprites), then Phase 5 (Android).