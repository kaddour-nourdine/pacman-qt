#pragma once

#include <vector>

#include "pacman/Events.hpp"
#include "pacman/Ghost.hpp"
#include "pacman/Level.hpp"
#include "pacman/Maze.hpp"
#include "pacman/Player.hpp"
#include "pacman/Random.hpp"

namespace pac {

enum class GameState {
    Ready,
    Running,
    PlayerDied,
    LevelComplete,
    GameOver,
};

enum class GlobalMode {
    Scatter,
    Chase,
};

const char* toString(GameState state);
const char* toString(GlobalMode mode);

class ModeSchedule {
public:
    ModeSchedule();
    explicit ModeSchedule(std::vector<float> phaseDurations);

    GlobalMode modeAt(float seconds) const;
    float phaseDuration(int index) const;
    int phaseCount() const;
    bool usesCustomSchedule() const { return custom_; }

private:
    std::vector<float> durations_;
    bool custom_ = false;
};

class Game {
public:
    static constexpr int kPelletPoints = 10;
    static constexpr int kPowerPelletPoints = 50;
    static constexpr float kDeathPauseSeconds = 1.6f;
    static constexpr float kLevelCompletePauseSeconds = 2.0f;

    explicit Game(Level level);

    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    void reset();
    void start();
    void step(float deltaSeconds);

    GameState state() const { return state_; }
    bool isRunning() const { return state_ == GameState::Running; }
    bool isOver() const { return state_ == GameState::GameOver; }

    int score() const { return score_; }
    int lives() const { return lives_; }
    int levelNumber() const { return levelNumber_; }

    GlobalMode globalMode() const { return globalMode_; }
    float frightenedSecondsLeft() const { return frightenedSecondsLeft_; }
    bool isFrightened() const { return frightenedSecondsLeft_ > 0.0f; }
    int pelletsRemaining() const { return maze_.pelletsRemaining(); }

    const Maze& maze() const { return maze_; }
    const Player& player() const { return player_; }
    Player& player() { return player_; }
    const std::vector<Ghost>& ghosts() const { return ghosts_; }
    std::vector<Ghost>& ghosts() { return ghosts_; }

    void setRandomSeed(std::uint32_t seed) { random_.seed(seed); }

    std::vector<Event> drainEvents();

private:
    void beginLevel();
    void simulate(float deltaSeconds);
    void updateFrightened(float deltaSeconds);
    void applyModes();
    void movePlayer(float deltaSeconds);
    void moveGhosts(float deltaSeconds);
    void collectPellet();
    void checkCollisions();
    void killPlayer();
    void finishLevel();
    void endGame();
    void releaseGhostsIfReady();
    float speedFor(const Ghost& ghost) const;
    void pushEvent(Event event);

    Level level_;
    Maze maze_;
    Player player_;
    std::vector<Ghost> ghosts_;
    Random random_;
    ModeSchedule schedule_;
    GameState state_ = GameState::Ready;
    GlobalMode globalMode_ = GlobalMode::Scatter;
    int score_ = 0;
    int lives_ = 0;
    int levelNumber_ = 1;
    float scheduleSeconds_ = 0.0f;
    float frightenedSecondsLeft_ = 0.0f;
    float stateSeconds_ = 0.0f;
    std::vector<Event> events_;
};

}