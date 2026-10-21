# todo.md — next steps

Roadmap for `pacman-qt`. Check items off as you finish them; add new items to the
bottom of the relevant section.

Legend: `[ ]` open · `[x]` done · `[!]` blocked · sizes **S** (< 1 h) / **M** (half a day) / **L** (multi-day)

---

## Where the project is right now

| | |
| --- | --- |
| Model (`core/`) | complete: level parsing, level list (`LevelManager`), maze rules, player, 4 ghosts, scoring, lives, mode schedule, events, fixed-timestep engine |
| Level data | three playable levels — `levels/level1.txt` is the classic arcade maze, plus `levels/level2.txt` and `levels/level3.txt` |
| Tests | 139 core tests, 12 CTest suites (incl. `QtView`), all passing |
| Docs | `README.md` + `docs/` (architecture, code tour, gameplay rules, level format, testing, coverage, performance, view implementation) |
| Tools | `tools/pacman_text`, `tools/ai_demo`, `tools/pacman_fuzz`, `tools/pacman_bench`, `tools/coverage.sh` |
| View | `app/` Qt Quick app — `GameController` bridge, classic vector arcade rendering, sound effects, high score + settings persistence, keyboard + touch input, pause menu, headless QTest |
| Android | build pipeline complete + device tested (touch, audio mixer, lifecycle, safe areas, back button, settings, free orientation) |
| Git | repository on `main`, pushed to `origin` |
| CI | `.github/workflows/ci.yml` — core job + Qt job (Linux) |

Build and test, for reference:

```sh
cmake -S . -B build && cmake --build build -j && ctest --test-dir build --output-on-failure
cmake -S . -B build -DPACMAN_BUILD_APP=ON -DPACMAN_BUILD_QT_TESTS=ON && cmake --build build -j
```

---

## Completed (summary)

- **Phase 0–1**: git init, core gaps closed (duplicate ghost marker, parse-every-level test, soak test, pellet-based ghost release, frightened flash, corner-turn restriction).
- **Phase 2**: `LevelManager` campaign, levels 2 & 3, difficulty curve, fruit tiles, extra-life thresholds.
- **Phase 3**: terminal renderer `pacman_text`, AI demo, reachability validator.
- **Phase 4**: Qt Quick view (GameController bridge, vector rendering, HUD, audio mixer, headless QTest).
- **Phase 5**: Android pipeline (NDK, Qt 6.12 kit, signed APK, bundled levels, touch, audio mixer, lifecycle, safe areas, back button, settings, slow mode, free orientation).
- **Phase 6**: coverage (98.1 % lines), parser fuzzer, determinism regression, performance baseline, install/export rules, CI workflow, docs sync.

---

## Open items

### Housekeeping
- [x] **Code style** (S): added `.clang-format` (LLVM-based, 4-space, 100-col, Attach braces, left-aligned pointers) and applied it to `core/` and `tests/`. *Done when:* `clang-format --dry-run -Werror` is clean.

### Android polish
- [ ] **Release signing** (S): replace the shared debug keystore with a real key.
- [x] **Perf diagnostics** (S): removed the frame-pacing probes (`pacman-perf: tick gap …` / `render-thread gap …`) and the `sfx trigger blocked` tripwire now that the audio investigation is closed; kept the audio health counters (`voicesStarted` / `framesWritten` / `peak`) that guard against a silent mixer.
- [x] **Extra audio effects** (M): added `ghost_eaten.wav` (rising arpeggio) and `death.wav` (descending tone), wired through `SfxPlayer::playGhostEaten()` / `playDeath()` and the QML `ghostEaten` / `playerDied` signals.
- [x] **Desktop display check** (S): verified the Qt app on a real desktop display (Wayland/X11) — window opens at 640×760, audio device detected, no crash.

### Model polish (documented as intentional differences)
- [x] **Ghost house release timers** (M): released ghosts now use `GhostMode::LeavingHouse` to exit through the door before switching to normal AI; unreleased ghosts stay in the house. (Still pellet-based thresholds: Blinky 0, Pinky 30, Inky 60, Clyde 90.)
- [x] **Blinky bounce quirk** (S): disabled `forbidUpwardTurn()` (returns `false`); the restriction was tuned for an older layout and made Blinky reverse into the house. Determinism golden value regenerated.
- [x] **Pixel-accurate collision** (M, optional): **deferred by decision** — keep tile-based collision as designed (documented in `docs/gameplay-rules.md` §7). Revisit only as a deliberate fidelity decision.

### Long-term
- [ ] **CI expansion** (M): add a job that builds the Qt app under `qt-qemu`/offscreen (already covered by the offscreen job; qemu optional).
- [ ] **Docs sync** (ongoing): new rules → `docs/gameplay-rules.md`, new files → `docs/code-tour.md`.

---

## Decisions (resolved)

1. **Level file format** — **keep minimal.** The current format is deliberately minimal and well-tested; add richness only when needed.
2. **Tile vs pixel collision** — **tile.** Kept tile-based by design (deferred, documented in `docs/gameplay-rules.md`).
3. **Chain vs per-ghost scoring** — **per-ghost.** Current implementation (points by ghost id) retained.
4. **View location** — **one `app/`.** Single `app/` for desktop + Android; simpler.
5. **High score/settings storage** — **view-local (`QSettings`).** Model stays I/O-free.
6. **Campaign ownership** — **view drives.** `LevelManager` lives in the view; `Game` takes a single `Level`.

---

## If you only have an hour

1. Extra audio effects: add ghost-eaten and death sounds (minor polish).
2. Release signing: create a real key and document the flow.
3. Desktop display check: run the Qt app on a real display.