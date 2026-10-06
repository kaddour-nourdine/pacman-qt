#pragma once

namespace pac {

enum class EventType {
    LevelStarted,
    PelletEaten,
    PowerPelletEaten,
    FruitEaten,
    GhostEaten,
    GhostRespawned,
    PlayerDied,
    LevelCompleted,
    GameOver,
};

struct Event {
    EventType type = EventType::LevelStarted;
    int x = -1;
    int y = -1;
    int points = 0;
    int value = 0;
};

}