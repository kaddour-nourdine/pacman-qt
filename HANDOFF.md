# HANDOFF — current state

**Project status: feature-complete and ready for external review** (pending newly
discovered bugs).

Short snapshot of the project state for the next session. `todo.md` holds the plan
and open items; `docs/` holds the explanations; this file holds only the state and
the constraints.

## Verified state

| | |
| --- | --- |
| Working directory | `/home/<you>/Development/pacman-qt` |
| Branch | `main`, in sync with `origin/main` |
| Build | clean, no warnings with `-Wall -Wextra -Wpedantic` (and `-Werror`) |
| Tests | 139 core tests + `QtView` (12 CTest suites), all passing |
| Toolchain | CMake 4.3.0, g++ (GCC) 16.2.1, Linux, 16 cores |
| Qt | 6.12 (`~/Qt/6.12.0/gcc_64` for desktop, `android_arm64_v8a` for Android) |
| View | `app/` builds with `-DPACMAN_BUILD_APP=ON`; runs on a real display and headless (`QT_QPA_PLATFORM=offscreen`) |
| Device tested | Tecno A059 (Android 16, arm64-v8a, fr-BE): touch, audio mixer, lifecycle, safe areas, back button, settings, free orientation |
| CI | `.github/workflows/ci.yml` (core + Qt jobs, Linux) |

`build/` exists and is current, so a rebuild is incremental.

## Constraints

- Work only inside this directory. Do not probe or modify anything outside it, and do
  not install system packages without asking.
- `core/` must stay free of Qt, graphics, platform APIs, wall-clock time and file I/O
  other than reading level files (`Level::loadFile`, `LevelManager::loadDirectory`).
- Game rules live in `Game`/`Maze`/`Ghost`; the view may only read state, send input
  requests and react to `Event`s.
- The model is deterministic: fixed 1/120 s steps, seeded xorshift RNG, no `rand()`.

## Quick start

```sh
cd /home/<you>/Development/pacman-qt
git status && git log --oneline -5
cmake -S . -B build -DPACMAN_BUILD_APP=ON -DPACMAN_BUILD_QT_TESTS=ON && cmake --build build -j
ctest --test-dir build --output-on-failure
QT_QPA_PLATFORM=offscreen ./build/app/pacman_app
```

## Still open

Release signing with a real key, the perf-diagnostics keep/remove decision, extra audio
effects, the code-style decision, and the model polish items listed in `todo.md`. See
`todo.md` for the full list and the open decisions.