# View Implementation (Phase 4)

This document describes the Qt Quick view implementation for pacman-qt, which was completed in **Phase 4**. It covers the architecture, components, and design decisions that bring the core model to life as a playable game.

> **Status**: Phase 4 is **complete** — the Qt Quick view builds, runs, and passes headless tests.
> Phase 5 extended the same view layer for Android: lifecycle (suspend/resume), safe-area
> insets, the back button and persistent settings all live here, not in the model.

---

## Overview

The view layer (`app/`) is a **Qt 6 Quick / QML** application that:

1. Loads and manages the campaign via `LevelManager` (from the source tree when present,
   otherwise from the `levels/*.txt` files embedded as Qt resources — see
   `GameController::materializeBundledLevels()`)
2. Drives the simulation with a 60fps timer
3. Renders the maze and actors using vector graphics on a `Canvas`
4. Handles keyboard input (arrows, WASD, Space, P, Esc, R) and touch input (swipe, tap)
5. Plays sound effects in response to game events (C++ `SfxPlayer`: one persistent
   `QAudioSink` fed by a software mixer, so triggering a sound never blocks the UI thread)
6. Persists the high score and the user settings (sound, slow mode) using `QSettings`
7. Exposes the model to QML through a single bridge class (`GameController`)
8. Freezes the model while the app is in the background and resumes without a time spike
9. Opens a pause menu on the Android back button (through a JNI bridge) and on `Esc`
10. Keeps the layout clear of notches and system bars (safe-area insets)

The view is **completely separate** from the model (`core/`). It reads state, sends input requests, and reacts to events — it never implements game rules.

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        QML Layer (Main.qml)                         │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │  Window (640×760)                                              ││
│  │    ┌─────────────────────────────────────────────────────┐  ││
│  │    │  Column (root)                                        │  ││
│  │    │    ┌─────────────┐    ┌─────────────────────────┐    │  ││
│  │    │    │   HUD        │    │         Board            │    │  ││
│  │    │    │  Score/Hi    │    │    ┌─────────────────┐   │    │  ││
│  │    │    │  Lives/Level │    │    │   Canvas         │   │    │  ││
│  │    │    │  Overlays    │    │    │   (rendering)    │   │    │  ││
│  │    │    └─────────────┘    │    └─────────────────┘   │    │  ││
│  │    │                        │                         │    │  ││
│  │    │    ┌─────────────────────────────────────────────┐  │    │  ││
│  │    │    │  sfx.playMunch()/playPower() → SfxPlayer (C++) │  │    │  ││
│  │    │    │  Keys.onPressed handler                         │  │    │  ││
│  │    │    └─────────────────────────────────────────────┘  │    │  ││
│  │    └─────────────────────────────────────────────────────┘  ││
│  └─────────────────────────────────────────────────────────────┘│
└──────────────────────┬──────────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────────┐
│                    C++ Bridge Layer (GameController)                │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │  GameController : QObject                                      ││
│  │    ┌─────────────────┐    ┌──────────────────────────────┐  ││
│  │    │  Q_PROPERTIES    │    │         Q_INVOKABLE           │  ││
│  │    │  - score         │    │         - start()            │  ││
│  │    │  - highScore     │    │         - reset()            │  ││
│  │    │  - lives         │    │         - togglePause()      │  ││
│  │    │  - levelNumber   │    │         - requestDirection() │  ││
│  │    │  - levelName     │    │                                  │  ││
│  │    │  - stateName     │    │  ┌──────────────────────────┐│  ││
│  │    │  - paused        │    │  │  Qt Signals               ││  ││
│  │    │  - mazeData      │    │  │  - levelStarted           ││  ││
│  │    │  - itemData      │    │  │  - pelletEaten             ││  ││
│  │    │  - mazeCols/Rows  │    │  │  - powerPelletEaten        ││  ││
│  │    │  - playerX/Y     │    │  │  - fruitEaten              ││  ││
│  │    │  - playerMouth   │    │  │  - ghostEaten              ││  ││
│  │    │  - playerDir     │    │  │  - ghostRespawned          ││  ││
│  │    │  - ghosts        │    │  │  - playerDied             ││  ││
│  │    └─────────────────┘    │  │  - levelCompleted          ││  ││
│  │                            │  │  - gameOver                ││  ││
│  │    ┌─────────────────┐    │  └──────────────────────────┘│  ││
│  │    │  Private Members │                                       │  ││
│  │    │  - levels_       │  LevelManager (campaign)        │  ││
│  │    │  - game_         │  Game (model)                    │  ││
│  │    │  - engine_       │  Engine (fixed timestep)        │  ││
│  │    │  - timer_        │  QTimer (16ms = ~60fps)         │  ││
│  │    │  - frameClock_   │  QElapsedTimer (frame timing)   │  ││
│  │    │  - previous*     │  Position snapshots for interp │  ││
│  │    └─────────────────┘                                       │  ││
│  └─────────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────────┐
│                      Model Layer (pacman_core)                      │
│  LevelManager → Level → Maze → Entity → {Player, Ghost} → Game     │
│  Events ← Engine (fixed 1/120s steps)                             │
└─────────────────────────────────────────────────────────────────┘
```

### Phase 5 additions (Android)

Phase 5 did not add a second view — it extended this one with three small
helpers and a lifecycle state. They are all in `app/`, all optional on the
desktop, and none of them touch `core/`:

```
   Android activity (Java)            C++ (app/)                    QML (Main.qml)
   ───────────────────────            ───────────                    ──────────────
   MainActivity                        GameController                game   (model)
     back press  ──JNI──▶  AndroidBridge ──backPressed()──▶  pause menu
     onPause/onResume ──▶ applicationStateChanged ──▶ suspended
                                       AppSettings ◀──toggleSound/toggleSlowMode──▶ pause menu
   QWindow::safeAreaMargins ──▶ ScreenInsets ───────────────────────────▶ insets (margins)
```

| File | Role |
| --- | --- |
| `app/AppSettings.{hpp,cpp}` | Persistent user settings (`QSettings`): sound on/off, slow mode. Owned by `GameController`, exposed to QML as `game.settings`. |
| `app/ScreenInsets.{hpp,cpp}` | Follows `QWindow::safeAreaMargins()` (Qt 6.9+, guarded by a `QT_VERSION` check) and publishes it as `insets` for the QML layout. |
| `app/AndroidBridge.{hpp,cpp}` | Receives the back press from the Java activity through JNI and re-emits it as a Qt signal. A no-op on the desktop. |
| `android/package/src/org/pacman/qt/MainActivity.java` | The activity: free orientation, immersive mode, back-button interception. |

---

## Build Configuration

The view is **optional** — the core model builds without Qt. Two CMake options control the view:

| Option | Default | Purpose |
| --- | --- | --- |
| `PACMAN_BUILD_APP` | `OFF` | Build the Qt Quick application (`pacman_app`) and the `pacman_view` library |
| `PACMAN_BUILD_QT_TESTS` | `OFF` | Build the headless Qt view test (`pacman_qttest`, requires `PACMAN_BUILD_APP=ON`) |

### Building with the View

```sh
# Configure with Qt support
cmake -S . -B build -DPACMAN_BUILD_APP=ON

# Build everything (core + view + tools)
cmake --build build -j

# Build with Qt view tests
cmake -S . -B build -DPACMAN_BUILD_APP=ON -DPACMAN_BUILD_QT_TESTS=ON
cmake --build build -j
```

### Qt Dependencies

The view requires **Qt 6** with these components:
- `Qt6::Quick` — QML and Canvas rendering
- `Qt6::QuickControls2` — Window management
- `Qt6::Multimedia` — persistent `QAudioSink` + software mixer (`SfxPlayer`/`SfxMixer`)
- `Qt6::Test` — For the headless view test (optional)

On Fedora:
```sh
sudo dnf install qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtmultimedia-devel
```

---

## Component Details

### 1. `app/CMakeLists.txt`

The build file creates **two targets**:

1. **`pacman_view`** — A static library containing:
   - `GameController` class (C++ bridge)
   - `AppSettings` (persistent user settings), `ScreenInsets` (safe area) and
     `AndroidBridge` (JNI back-button bridge)
   - QML module (`PacMan` URI, version 1.0, `NO_PLUGIN`)
   - Qt resources (`pacman_assets`): SVG sprites and WAV sound effects

2. **`pacman_app`** — A thin executable (`main.cpp`) that links `pacman_view`

**Resource Files** (embedded via `qt_add_resources`):
- Sprites: `assets/sprites/*.svg` (tiles, pacman, ghosts, eyes, life icon)
- Icons: `assets/icons/pacman.svg`
- Audio: `assets/audio/munch.wav`, `assets/audio/power.wav`, `assets/audio/ghost_eaten.wav`, `assets/audio/death.wav`

The resources are embedded with prefix `/`, making them accessible via `qrc:/` paths in QML.

---

### 2. `app/main.cpp` — Entry Point

A minimal Qt application entry point that:

1. Creates `QGuiApplication` with organization name `pacman-qt` and app name `pacman`
2. Sets the window icon from the embedded resource
3. Creates `GameController`, `SfxPlayer`, `ScreenInsets` and `AndroidBridge` on the stack
4. Wires the sound setting to the mixer (`settings.soundEnabledChanged` → `sfx.setEnabled()`)
5. Sets up `QQmlApplicationEngine` with four context properties:
   `game` (the controller), `sfx`, `insets` and `android` (the bridge)
6. Connects error handlers for QML loading failures
7. Loads `Main.qml` from the `PacMan` module
8. Attaches the `ScreenInsets` to the root window (safe area) and hooks the
   render-thread cadence probe
9. Returns -1 if the root object fails to create

**Key Design Decision**: The controller is created **before** the QML engine, ensuring `QSettings` reads the correct organization/application names for high score persistence.

---

### 3. `GameController` — The C++/QML Bridge

Located in `app/GameController.hpp` and `app/GameController.cpp`.

#### Responsibilities

- **Owns the model**: `LevelManager`, `Game`, and `Engine` instances; the campaign comes from
  `PACMAN_LEVELS_DIR` (source tree) or, when that path does not exist (installed app, phone),
  from the `:/levels/*` resources copied into `QStandardPaths::AppDataLocation`
- **Frame loop**: 16ms `QTimer` drives `Engine::update()` with real frame time
- **State synchronization**: Snapshots model state and exposes it via Q_PROPERTIES
- **Event forwarding**: Converts `Event` structs from the model into Qt signals
- **Interpolation**: Uses `Engine::interpolationAlpha()` for smooth rendering
- **Persistence**: Saves/loads high score via `QSettings`, and owns the `AppSettings`
- **Lifecycle**: Suspends the model when the app goes to the background (see below)

#### Properties (Q_PROPERTY)

| Property | Type | Source | Description |
| --- | --- | --- | --- |
| `score` | `int` | `game_.score()` | Current score |
| `highScore` | `int` | QSettings | Persisted high score |
| `lives` | `int` | `game_.lives()` | Remaining lives |
| `levelNumber` | `int` | `game_.levelNumber()` | Current level (1-based) |
| `levelName` | `QString` | `game_.levelName()` | Level label from LevelManager |
| `pelletsRemaining` | `int` | `game_.pelletsRemaining()` | Pellets left to eat |
| `stateName` | `QString` | Game state | `"ready"`, `"running"`, `"died"`, `"levelComplete"`, `"gameOver"` |
| `paused` | `bool` | Internal | Pause state (affects time scale) |
| `suspended` | `bool` | `QGuiApplication::applicationStateChanged` | The app is in the background: the model is frozen and the tick timer is stopped |
| `settings` | `AppSettings*` | Owned member | The persistent settings (`soundEnabled`, `slowMode`) |
| `mazeData` | `QString` | Maze tiles | String representation of walls/doors/tunnels |
| `itemData` | `QString` | Maze items | String representation of pellets/power/fruit |
| `mazeCols` / `mazeRows` | `int` | `game_.maze()` | Maze dimensions |
| `playerX` / `playerY` | `double` | Interpolated | Player position (with tunnel wrap handling) |
| `playerMouth` | `double` | `player.animationPhase()` | Mouth animation phase (0-1) |
| `playerDirection` | `int` | `player.direction()` | Current direction (0-4, cast from Direction enum) |
| `ghosts` | `QVariantList` | Interpolated | Array of ghost data (x, y, id, eyes, edible, flashing) |

#### Invokable Methods (Q_INVOKABLE)

| Method | Parameters | Description |
| --- | --- | --- |
| `start()` | — | Starts game from Ready state, or resets if GameOver |
| `reset()` | — | Resets to level 1 with full lives |
| `togglePause()` | — | Toggles pause (only effective in Running state) |
| `requestDirection(int)` | direction (0-4) | Queues a direction request (Up=1, Right=2, Down=3, Left=4) |

#### Qt Signals

Every `EventType` from the model is re-emitted as a Qt signal:
- `levelStarted(int levelNumber)`
- `pelletEaten(int x, int y)`
- `powerPelletEaten(int x, int y)`
- `fruitEaten(int points, int x, int y)`
- `ghostEaten(int points, int x, int y)`
- `ghostRespawned()`
- `playerDied()`
- `levelCompleted()`
- `gameOver()`

Additionally:
- `statsChanged()` — Emitted when score, lives, level, or pellets change
- `highScoreChanged()` — Emitted when a new high score is achieved
- `stateChanged()` — Emitted when game state changes
- `pausedChanged()` — Emitted when pause state changes
- `suspendedChanged()` — Emitted when the app is suspended or resumed
- `mazeChanged()` — Emitted when maze geometry changes
- `itemsChanged()` — Emitted when pellet/fruit state changes
- `actorsChanged()` — Emitted every frame after position updates

#### Lifecycle (suspend / resume)

The Android platform reports backgrounding through
`QGuiApplication::applicationStateChanged`. The controller listens to it and
suspends on `Qt::ApplicationSuspended` **only** — `ApplicationInactive` also
covers transient interruptions (a system dialog, a notification shade), and
freezing the game there would be wrong on the desktop as well.

Suspending does three things:

1. `timer_.stop()` — the tick timer stops entirely, so a backgrounded app
   costs nothing.
2. `frameClock_.restart()` on resume — the time spent in the background is
   thrown away instead of arriving as one huge frame (the engine would
   otherwise have to catch up).
3. The time scale goes to 0 (paused *or* suspended), so the model does not
   advance while the app is away.

The hitch detector is reset at the same time, so a suspend is not reported as
a frame gap.

#### Time scale (slow mode)

The engine's time scale is derived in one place, `applyTimeScale()`:

| State | Time scale |
| --- | --- |
| Running, normal speed | 1.0 |
| Running, slow mode | 0.8 (`kSlowFactor`) |
| Paused (menu) | 0.0 |
| Suspended (background) | 0.0 |

Slow mode scales the *model*, not the frame rate: the simulation stays
deterministic, it just advances with fewer steps per second. The switch lives
in `AppSettings` and is reachable from the pause menu.

#### Implementation Details

**Frame Loop (`tick()` method):**
```cpp
void GameController::tick() {
    capturePositions();                    // Snapshot current positions
    const double frameSeconds = ...;      // Measure real time
    frameClock_.restart();
    engine_.update(frameSeconds);         // Advance simulation
    drainModelEvents();                   // Convert Events to Qt signals
    syncMaze();                           // Update mazeData/itemData
    syncStats();                          // Update score/lives/level
    syncState();                          // Update stateName
    emit actorsChanged();                 // Trigger repaint
}
```

**Interpolation:**
- Positions are interpolated between the previous and current frame using `Engine::interpolationAlpha()`
- Special handling for tunnel wraps: if the distance between previous and current exceeds half the maze width, the position snaps to current (no interpolation across tunnels)
- Ghosts and player are interpolated separately

**Event Drain:**
- `drainModelEvents()` iterates through all events from `game_.drainEvents()`
- Each event type maps to a corresponding Qt signal
- `GameOver` event also checks and updates the high score

**Maze Data Encoding:**
- `mazeData`: String where each character represents a tile column-wise
  - `#` = Wall
  - `-` = Door
  - `_` = Tunnel
  - ` ` (space) = Empty/Walkable
- `itemData`: String where each character represents an item
  - `.` = Pellet
  - `o` = Power Pellet
  - `F` = Fruit
  - ` ` (space) = Nothing

**Ghost Data Encoding:**
Each ghost is encoded as a `QVariantMap` with keys:
- `x` (double): Interpolated X position
- `y` (double): Interpolated Y position
- `id` (int): Ghost ID (0=Blinky, 1=Pinky, 2=Inky, 3=Clyde)
- `eyes` (bool): Whether ghost is in Eyes mode (eaten, returning home)
- `edible` (bool): Whether ghost is frightened (can be eaten)
- `flashing` (bool): Whether ghost is in the final 2 seconds of frightened mode

---

### 4. `Main.qml` — The QML User Interface

The complete UI in a single QML file with these components:

#### Window
- Size: 640×760 pixels
- Title: "pacman-qt"
- Background: Black (#000000)

#### Layout Structure
```
Window
└── Column (rootColumn)
    ├── Item (hud) — 60px tall
    │   ├── Row (left HUD) — Score, High Score, Level
    │   └── Row (right HUD) — Lives (as SVG icons)
    └── Item (boardArea) — Remaining space
        ├── Canvas (board) — Game rendering
        ├── Text overlays — READY!, LEVEL CLEAR, GAME OVER, PAUSED, NO LEVELS FOUND
        ├── Text (popup) — Score popups with fade animation
        ├── Connections — forwards pellet events to the C++ `sfx` player
        └── Keys.onPressed — Input handler
```

#### HUD (Head-Up Display)

**Left Side:**
- Score: White text, 18px bold
- High Score: White text, 18px bold
- Level: White text, 18px bold
- Labels: Gray text (#888888), 11px

**Right Side:**
- Lives: Repeater with `life.svg` images (20×20px)

#### Canvas Rendering

The `Canvas` element draws everything using vector graphics:

**Helper Functions:**
- `circle(ctx, x, y, r, color)` — Draws a filled circle
- `drawEyes(ctx, px, py, c)` — Draws ghost eyes (white with black pupils)
- `drawPlayer(ctx, c)` — Draws Pac-Man as a yellow wedge with animated mouth
- `drawGhost(ctx, c, g)` — Draws ghost body (dome + rectangle) and eyes

**Color Scheme:**
- Walls: Blue (#0000ff) outlined rectangles
- Door: Pink (#ffb8de) filled rectangle
- Pellets: White (#ffffff) small circles
- Power Pellets: White (#ffffff) larger circles
- Fruit: Red (#ff3131) circles
- Pac-Man: Yellow (#ffff00), Red (#ff0000) when dying
- Ghosts: Red (#ff0000), Pink (#ffb3b3), Cyan (#00ffff), Orange (#ff9900)
- Frightened Ghosts: Blue (#0000ff), White (#ffffff) when flashing

**Rendering Order:**
1. Clear canvas to black
2. Draw walls, doors, tunnels
3. Draw pellets, power pellets, fruit
4. Draw Pac-Man
5. Draw ghosts (with eyes on top)

**Cell Sizing:**
- `cell` = `Math.max(Math.min(boardArea.width / cols, boardArea.height / rows), 4)`
- Ensures cells are at least 4px and square
- Canvas size = `cols * cell` × `rows * cell`

#### Overlays

Centered text overlays appear based on `game.stateName`:
- `"ready"` → "READY!" (yellow)
- `"levelComplete"` → "LEVEL CLEAR" (cyan)
- `"gameOver"` → "GAME OVER" (red)
- `paused` → "PAUSED" (white)
- `mazeCols === 0` → "NO LEVELS FOUND" (red)

#### Score Popups

- Triggered by `ghostEaten` and `fruitEaten` signals
- Shows `"+" + points` at the ghost/fruit position
- Fades out over 900ms using `NumberAnimation`
- Positioned at `(tileX + 0.5) * cell - width/2` horizontally, `tileY * cell - cell * 0.4` vertically

#### Sound Effects

Audio is played by the C++ player, not by QML `SoundEffect` objects. `Main.qml` only
forwards the events:

```qml
Connections {
    target: game
    function onPelletEaten(x, y) {
        sfx.playMunch()      // cheap: playback happens in SfxPlayer's thread
    }
    function onPowerPelletEaten(x, y) {
        sfx.playPower()
    }
}
```

`app/SfxPlayer.{hpp,cpp}` owns the audio path:

| Piece | Role |
| --- | --- |
| `SfxPlayer` | QML-facing object (`sfx` context property): opens the sink, decodes the WAVs, exposes `playMunch()` / `playPower()` / `playGhostEaten()` / `playDeath()` and the health counters |
| `SfxMixer` | a `QIODevice` that mixes up to 4 voices (linear resampling) into the sink's format (Int16/Int32/Float/UInt8) |
| `QAudioSink` | opened **once** at startup and fed until the app exits — nothing is started or stopped during gameplay |

Why not `SoundEffect`: on Android every `QSoundEffect::play()` restarted an
`AudioTrack`, which stalled the whole frame pipeline (~50 tick gaps per game,
`ANDROID_BUILD_STATUS.md` §7).

Two details that are easy to get wrong (both cost a debugging round on the phone):

- **`bytesAvailable()` must be overridden.** Qt's audio pipeline only pulls from a user
  `QIODevice` when it reports data (`pullFromQIODeviceToRingbuffer()` bails out on
  `bytesAvailable() == 0`); a plain `QIODevice` reports 0 forever, so the sink idled and
  the game was silent (`ANDROID_BUILD_STATUS.md` §8). The mixer is a generator: it always
  claims a 200 ms chunk.
- **Health counters.** `voicesStarted()` (sounds queued), `framesWritten()` (frames that
  actually left the mixer) and `peakLevel()` (loudest sample) are logged at startup,
  every 10 s while playing and on sink-state changes — a silent mixer is not an error,
  so it has to be measured. `ViewTest.testMixerFeedsTheAudioSink` guards the regression.

Mixing applies the old `SoundEffect` volume (`kSfxGain = 0.6`) and clamps the sum to
[-1, 1] — two overlapping munches would otherwise exceed full scale.

#### Input Handling

`Keys.onPressed` handler maps keys to controller methods:

| Key | Action |
| --- | --- |
| Up, W | `requestDirection(1)` — Up |
| Right, D | `requestDirection(2)` — Right |
| Down, S | `requestDirection(3)` — Down |
| Left, A | `requestDirection(4)` — Left |
| Space, Return | `start()` — Start or restart |
| P | `togglePause()` — Pause/resume |
| Escape, Back | Open/close the pause menu |
| M | Toggle the sound setting |
| R | `reset()` — Reset game |

Direction values match the `Direction` enum:
- `None = 0`
- `Up = 1`
- `Right = 2`
- `Down = 3`
- `Left = 4`

#### The Pause Menu

`Main.qml` has a full-screen overlay (`menuOpen`) with the entries
`RESUME`, `SOUND: ON/OFF`, `SPEED: NORMAL/SLOW` and `RESTART`. It is
reachable in three ways:

- the `≡` button in the HUD (the only way on a phone),
- the `Escape` or `Back` key,
- the Android back button, through the `AndroidBridge` (see below).

The menu pauses a running game and restores the previous state when it closes
(`menuPausedTheGame`). The overlay swallows touches, so a tap on the menu
does not steer Pac-Man.

#### Touch Input

The board carries a `MouseArea` that maps touch to the model: a swipe
(dead zone `max(30px, 8% of the board)`) turns Pac-Man, a tap is `Space`
(start/restart). The origin re-arms while the finger is down, so a continuous
drag keeps steering.

---

## Design Decisions

### 1. Vector Rendering vs. Sprites

**Decision**: Use vector graphics (Canvas) instead of SVG sprites.

**Rationale**:
- The original GTK pacman used vector rendering
- Provides authentic arcade look
- Scales perfectly to any resolution
- No need to manage sprite sheets or individual image files
- Simpler asset management (only need SVG for life icon)

**Implementation**:
- Walls: Outlined rectangles with blue stroke
- Pac-Man: Yellow wedge with animated mouth (using `arc`)
- Ghosts: Dome (semicircle) + body (rectangle) combination
- Eyes: White circles with black pupils

### 2. Single Bridge Class

**Decision**: Only `GameController` includes both Qt and `core/` headers.

**Rationale**:
- Keeps `core/` completely Qt-free and portable
- Single, well-defined boundary between view and model
- Easy to test (headless Qt test links the same library)
- Clear separation of concerns

### 3. Property-Based Data Flow

**Decision**: Expose all viewable state via Q_PROPERTY with NOTIFY signals.

**Rationale**:
- QML can bind directly to properties
- Automatic change notification
- Declarative UI updates
- No polling required

### 4. Event-Driven Signals

**Decision**: Re-emit every model Event as a Qt signal.

**Rationale**:
- View reacts to game events, not state changes
- Enables sound effects, animations, popups
- Consistent with model's event-based design
- Loose coupling between model and view

### 5. Interpolation for Smooth Motion

**Decision**: Interpolate positions between simulation steps.

**Rationale**:
- Simulation runs at fixed 120Hz
- Display may run at variable frame rates (60Hz, 144Hz, etc.)
- Interpolation provides smooth motion regardless of display refresh rate
- Uses `Engine::interpolationAlpha()` to get the fraction through the current step

**Special Case**: Tunnel wraps are detected by checking if the distance between previous and current positions exceeds half the maze width. In this case, the position snaps to the current position (no interpolation across the tunnel).

### 6. Static Library for View Code

**Decision**: Compile view code into `pacman_view` static library.

**Rationale**:
- Headless test (`pacman_qttest`) can link the same code as the app
- Thin `pacman_app` executable (only `main.cpp`)
- Clean separation: app is just an entry point

### 7. High Score in View Layer

**Decision**: Persist high score in `GameController` using `QSettings`.

**Rationale**:
- Model has no file I/O by design
- High score is a view concern (display, persistence)
- `QSettings` provides platform-appropriate storage
- Uses organization name `pacman-qt` and app name `pacman`

The same reasoning applies to the user settings (sound, slow mode): they live
in `app/AppSettings`, a small `QSettings`-backed object owned by the
controller and reachable from QML as `game.settings`. Keeping them in one
place means the sound switch has exactly one source of truth — the mixer
obeys the setting (`SfxPlayer::setEnabled()`), it does not own it.

### 8. The Android Back Button Needs a JNI Bridge

**Decision**: Do not rely on Qt delivering the back press as a key event.

**Rationale**: Android hands the back press to the *activity*, and Qt's own
input delegate consumes it — on API 33+ it registers an
`OnBackInvokedCallback`, on older versions it swallows the `KEYCODE_BACK`
key event. Whether it reaches the Qt window as `Qt::Key_Back` is not
something the app can rely on.

**Implementation**: The activity (`android/package/src/org/pacman/qt/MainActivity.java`)
intercepts the press — `dispatchKeyEvent` for the hardware button and the
predictive-back callback for API 33+ — and calls the native method
`nativeBackPressed()`. That symbol
(`Java_org_pacman_qt_MainActivity_nativeBackPressed`, in
`app/AndroidBridge.cpp`) is exported by the app library, queues the call onto
the Qt thread and emits `backPressed()`. The QML side opens the pause menu.

The QML file still handles `Qt.Key_Back` (and `Escape`), so the desktop gets
the same menu — the Java side is only the Android path.

---

## Testing

### Headless Qt View Test

Located in `tests/test_view.cpp`, built only when `PACMAN_BUILD_QT_TESTS=ON`.

**Test**: `ViewTest::testLoadAndInput()`

1. Creates `GameController` and `QQmlApplicationEngine`
2. Loads `Main.qml` from the Qt resource system
3. Verifies the root object was created
4. Activates the window and waits 50ms
5. Calls `controller.start()`
6. Sends a Right arrow key event to the window
7. Waits 50ms for processing
8. Verifies the controller received the input by checking `directionForTest()` returns 2 (Right)

**Environment**: Runs with `QT_QPA_PLATFORM=offscreen` (no display required)

**Test**: `ViewTest::testBundledLevelsAreShipped()` — the bundled `levels/*.txt` resources are
copied into `QStandardPaths::AppDataLocation` when the build-host path is missing.

**Test**: `ViewTest::testMixerFeedsTheAudioSink()` — regression guard for the silent mixer
(Qt only pulls from a `QIODevice` that reports `bytesAvailable() > 0`): triggers a munch,
waits 500 ms and asserts that frames left the mixer and the peak level is > 0. Skipped
when the machine has no audio output device.

**Test**: `ViewTest::testBackKeyOpensThePauseMenu()` — loads `Main.qml`, sends a `Qt::Key_Back`
to the window and asserts that the QML property `menuOpen` became true (and that a second
press closes it again).

**Test**: `ViewTest::testPauseMenuEntriesToggleTheSettings()` — opens the menu, finds the
`SOUND: ON/OFF` and `SPEED: NORMAL/SLOW` entries in the item tree, clicks them and asserts
that the settings really change (the menu is the only way to reach them on a phone).

**Test**: `ViewTest::testAndroidBackBridgeOpensThePauseMenu()` — emits the
`AndroidBridge::backPressed()` signal (what the Java activity does through JNI) and asserts
that the pause menu opens.

**Test**: `ViewTest::testSuspendingFreezesTheModel()` — starts the game, suspends it, and
asserts that the player position and the score do not change while suspended, and that the
game runs again after the resume.

**Test**: `ViewTest::testSettingsArePersisted()` — writes the sound and slow-mode settings,
creates a fresh `AppSettings` and asserts that the values are read back.

**Test**: `ViewTest::testSlowModeScalesTheModel()` — measures how far the player moves in a
fixed time at normal speed and in slow mode, and asserts that slow mode moves less.

**CMake Integration**:
```cmake
add_test(NAME QtView COMMAND pacman_qttest)
set_tests_properties(QtView PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
```

### Running the Test

```sh
# Build with Qt tests
cmake -S . -B build -DPACMAN_BUILD_APP=ON -DPACMAN_BUILD_QT_TESTS=ON
cmake --build build -j

# Run the Qt view test
ctest --test-dir build -R QtView

# Or run directly
QT_QPA_PLATFORM=offscreen ./build/tests/pacman_qttest
```

---

## Running the Application

### Desktop

```sh
# Build with app support
cmake -S . -B build -DPACMAN_BUILD_APP=ON
cmake --build build -j

# Run the application
./build/app/pacman_app
```

### Headless Mode (for CI/testing)

```sh
QT_QPA_PLATFORM=offscreen ./build/app/pacman_app
```

### Controls

| Action | Keys |
| --- | --- |
| Move Up | ↑, W |
| Move Right | →, D |
| Move Down | ↓, S |
| Move Left | ←, A |
| Start/Restart | Space, Enter |
| Pause/Resume | P |
| Pause menu | Escape, Back |
| Toggle sound | M |
| Reset | R |

On a touch screen: swipe to turn, tap to start/restart, the `≡` button in the
HUD opens the pause menu, and the Android back button opens it as well.

---

## File Summary

| File | Purpose | Lines |
| --- | --- | --- |
| `app/CMakeLists.txt` | Build configuration for view library and app | 105 |
| `app/main.cpp` | Qt application entry point | 79 |
| `app/GameController.hpp` | Bridge class declaration (QObject) | 150 |
| `app/GameController.cpp` | Bridge class implementation | 463 |
| `app/AppSettings.hpp` | Persistent user settings (sound, slow mode) | 45 |
| `app/AppSettings.cpp` | Persistent user settings implementation | 33 |
| `app/ScreenInsets.hpp` | Safe-area insets helper | 41 |
| `app/ScreenInsets.cpp` | Safe-area insets implementation | 28 |
| `app/AndroidBridge.hpp` | JNI back-button bridge | 33 |
| `app/AndroidBridge.cpp` | JNI back-button bridge implementation | 33 |
| `app/SfxPlayer.hpp` | Audio player + software mixer declaration | 124 |
| `app/SfxPlayer.cpp` | Audio player + software mixer implementation | 355 |
| `app/Main.qml` | Complete QML user interface | 585 |
| `android/package/src/org/pacman/qt/MainActivity.java` | Android activity (orientation, immersive mode, back button) | 131 |
| `android/package/AndroidManifest.xml` | The manifest that replaces the generated one | 69 |
| `tests/test_view.cpp` | Headless Qt view test | 282 |

---

## Verification Checklist

Phase 4 is considered complete when:

- [x] `app/` directory exists with all source files
- [x] `PACMAN_BUILD_APP=ON` builds successfully
- [x] `pacman_app` executable runs without errors
- [x] Window opens with correct dimensions (640×760)
- [x] Maze renders correctly (walls, pellets, etc.)
- [x] Pac-Man and ghosts are visible and animated
- [x] Keyboard input works (arrows, WASD)
- [x] Game starts with Space/Enter
- [x] Pause works with P/Esc
- [x] Reset works with R
- [x] Sound effects play (munch.wav, power.wav)
- [x] HUD shows score, high score, level, lives
- [x] Overlays appear (READY!, LEVEL CLEAR, GAME OVER, PAUSED)
- [x] Score popups appear when eating ghosts
- [x] High score persists between sessions
- [x] `PACMAN_BUILD_QT_TESTS=ON` builds the headless test
- [x] `ctest --test-dir build -R QtView` passes
- [x] All 12 CTest tests pass (11 core + 1 Qt view)

---

## Known Limitations

These are documented limitations of the Phase 4 implementation:

1. **Audio**: Four sound effects implemented (munch, power, ghost eaten, death). Further arcade sounds (start jingle, fruit, etc.) are TODO.
2. **Display Verification**: Verified on a real desktop display (Wayland/X11) as well as headless (`offscreen`); the window opens at 640×760 and renders without errors.
3. **Locale**: Level parsing uses `std::from_chars` to avoid locale issues with decimal separators (fixed in Phase 4).
4. **Sprite Sheet**: The planned packed sprite sheet was cancelled in favor of vector rendering.
5. **Touch Input**: Implemented in Phase 5 (swipe to turn, tap to start) and verified on a phone.
6. **Fullscreen**: The desktop window is fixed at 640×760; only the Android build is full-screen (immersive mode).
7. **Android on-device checks**: The back button, the immersive mode, the (now removed)
   orientation lock and the suspend/resume behaviour are implemented, covered by the
   headless tests and verified on a device.

---

## Differences from Original Arcade

The view implementation intentionally differs from the 1980 arcade in these ways:

1. **Vector Graphics**: Uses Canvas drawing instead of pixel art sprites
2. **Resolution**: Fixed 640×760 window (arcade was 224×288 rotated)
3. **Aspect Ratio**: Cells are square (arcade had non-square pixels)
4. **Color Scheme**: Simplified color palette
5. **Sound**: Only basic sound effects, not the full arcade audio

These differences are acceptable because:
- The **model** faithfully implements arcade rules (see `docs/gameplay-rules.md`)
- The view is a **representation**, not a pixel-perfect emulation
- Vector rendering provides better scalability for modern displays

---

## Next Steps

Phase 5 (Android) is done for the items that the todo list tracks — the
lifecycle, the safe areas, the back button and the settings all live in this
view layer, and the APK builds and installs.

The remaining work is listed in `todo.md`:

- **Phase 6 — long-term quality**: CI, coverage, a parser fuzzer, a
  determinism regression test and the `pacman_core` install/export rules.
- **Android polish**: a pass on a real phone for the new lifecycle/back-button
  behaviour, and the missing sounds (ghost eaten, death).
