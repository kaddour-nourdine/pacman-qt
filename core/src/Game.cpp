#include "pacman/Game.hpp"

#include <utility>

namespace pac {

const char* toString(GameState state) {
    switch (state) {
    case GameState::Ready:
        return "ready";
    case GameState::Running:
        return "running";
    case GameState::PlayerDied:
        return "player-died";
    case GameState::LevelComplete:
        return "level-complete";
    case GameState::GameOver:
        break;
    }
    return "game-over";
}

const char* toString(GlobalMode mode) {
    return mode == GlobalMode::Scatter ? "scatter" : "chase";
}

ModeSchedule::ModeSchedule()
    : durations_{7.0f, 20.0f, 7.0f, 20.0f, 5.0f, 20.0f, 5.0f, 20.0f},
      custom_(false) {
}

ModeSchedule::ModeSchedule(std::vector<float> phaseDurations) : custom_(false) {
    if (phaseDurations.empty()) {
        durations_ = ModeSchedule().durations_;
        return;
    }
    durations_ = std::move(phaseDurations);
    custom_ = true;
}

GlobalMode ModeSchedule::modeAt(float seconds) const {
    if (durations_.empty()) {
        return GlobalMode::Scatter;
    }
    float elapsed = seconds < 0.0f ? 0.0f : seconds;
    for (std::size_t i = 0; i < durations_.size(); ++i) {
        if (elapsed < durations_[i]) {
            return (i % 2 == 0) ? GlobalMode::Scatter : GlobalMode::Chase;
        }
        elapsed -= durations_[i];
    }
    return (durations_.size() % 2 == 0) ? GlobalMode::Chase : GlobalMode::Scatter;
}

float ModeSchedule::phaseDuration(int index) const {
    if (index < 0 || index >= static_cast<int>(durations_.size())) {
        return 0.0f;
    }
    return durations_[static_cast<std::size_t>(index)];
}

int ModeSchedule::phaseCount() const {
    return static_cast<int>(durations_.size());
}

Game::Game(Level level) : Game(LevelManager(std::vector<Level>{std::move(level)})) {
}

Game::Game(LevelManager levels)
    : levels_(std::move(levels)),
      level_(levels_.levelFor(1)),
      maze_(level_),
      player_(maze_),
      schedule_(level_.config().phaseDurations),
      lives_(level_.config().startingLives) {
    createGhosts();
    beginLevel();
}

void Game::createGhosts() {
    ghosts_.clear();
    ghosts_.reserve(Level::kGhostCount);
    for (int i = 0; i < Level::kGhostCount; ++i) {
        const GhostSpawn& spawn = level_.ghostSpawn(i);
        ghosts_.emplace_back(maze_, static_cast<GhostId>(i), spawn.x, spawn.y, spawn.direction, &random_);
    }
}

void Game::loadLevel(const Level& level) {
    level_ = level;
    maze_ = Maze(level_);
    player_ = Player(maze_);
    createGhosts();
    schedule_ = ModeSchedule(level_.config().phaseDurations);
}

void Game::reset() {
    score_ = 0;
    levelNumber_ = 1;
    loadLevel(levels_.levelFor(1));
    lives_ = level_.config().startingLives;
    beginLevel();
}

void Game::beginLevel() {
    if (levels_.isEmpty()) {
        endGame();
        return;
    }
    maze_.resetPellets();
    player_.reset();
    for (Ghost& ghost : ghosts_) {
        ghost.reset();
        ghost.setReleased(false);
    }
    if (!ghosts_.empty()) {
        ghosts_.front().setReleased(true);
    }
    scheduleSeconds_ = 0.0f;
    frightenedSecondsLeft_ = 0.0f;
    stateSeconds_ = 0.0f;
    globalMode_ = GlobalMode::Scatter;
    state_ = GameState::Ready;

    Event event;
    event.type = EventType::LevelStarted;
    event.x = player_.tileX();
    event.y = player_.tileY();
    event.value = levelNumber_;
    pushEvent(event);
}

void Game::start() {
    if (state_ == GameState::Ready) {
        state_ = GameState::Running;
    }
}

void Game::step(float deltaSeconds) {
    if (deltaSeconds <= 0.0f) {
        return;
    }

    if (state_ == GameState::Running) {
        simulate(deltaSeconds);
        return;
    }

    stateSeconds_ += deltaSeconds;

    if (state_ == GameState::PlayerDied) {
        if (stateSeconds_ < kDeathPauseSeconds) {
            return;
        }
        if (lives_ <= 0) {
            endGame();
            return;
        }
        player_.reset();
        for (Ghost& ghost : ghosts_) {
            ghost.reset();
        }
        scheduleSeconds_ = 0.0f;
        frightenedSecondsLeft_ = 0.0f;
        state_ = GameState::Ready;
        stateSeconds_ = 0.0f;
        return;
    }

    if (state_ == GameState::LevelComplete && stateSeconds_ >= kLevelCompletePauseSeconds) {
        ++levelNumber_;
        loadLevel(levels_.levelFor(levelNumber_));
        beginLevel();
    }
}

void Game::simulate(float deltaSeconds) {
    updateFrightened(deltaSeconds);
    movePlayer(deltaSeconds);
    collectPellet();
    releaseGhostsIfReady();
    moveGhosts(deltaSeconds);
    checkCollisions();

    if (state_ == GameState::Running && maze_.allPelletsEaten()) {
        finishLevel();
    }
}

void Game::updateFrightened(float deltaSeconds) {
    if (frightenedSecondsLeft_ > 0.0f) {
        frightenedSecondsLeft_ -= deltaSeconds;
        if (frightenedSecondsLeft_ < 0.0f) {
            frightenedSecondsLeft_ = 0.0f;
        }
    } else {
        scheduleSeconds_ += deltaSeconds;
    }
    applyModes();
}

void Game::applyModes() {
    globalMode_ = schedule_.modeAt(scheduleSeconds_);
    const bool frightened = frightenedSecondsLeft_ > 0.0f;
    for (Ghost& ghost : ghosts_) {
        if (ghost.isEyes()) {
            continue;
        }
        if (frightened) {
            ghost.setMode(GhostMode::Frightened);
            continue;
        }
        ghost.setMode(globalMode_ == GlobalMode::Scatter ? GhostMode::Scatter : GhostMode::Chase);
    }
}

void Game::movePlayer(float deltaSeconds) {
    player_.update(deltaSeconds);
}

float Game::speedFor(const Ghost& ghost) const {
    const LevelConfig& config = level_.config();
    if (ghost.isEyes()) {
        return config.eyesSpeed;
    }
    if (ghost.isFrightened()) {
        return config.frightenedSpeed;
    }
    if (maze_.isTunnel(ghost.tileX(), ghost.tileY())) {
        return config.tunnelSpeed;
    }
    return config.ghostSpeed;
}

void Game::moveGhosts(float deltaSeconds) {
    const Vec2 playerPosition = player_.position();
    const Direction playerDirection = player_.direction();
    const Vec2 blinkyPosition = ghosts_.empty() ? playerPosition : ghosts_.front().position();

    for (Ghost& ghost : ghosts_) {
        if (ghost.isEyes() && ghost.hasReachedSpawn()) {
            ghost.reset();
            ghost.setReleased(true);
            Event event;
            event.type = EventType::GhostRespawned;
            event.x = ghost.tileX();
            event.y = ghost.tileY();
            event.value = static_cast<int>(ghost.id());
            pushEvent(event);
            continue;
        }
        if (!ghost.isReleased() && !ghost.isEyes()) {
            if (!ghost.isFrightened()) {
                ghost.setMode(GhostMode::Scatter);
            }
            ghost.updateTarget(playerPosition, playerDirection, blinkyPosition);
            ghost.setSpeed(speedFor(ghost));
            ghost.update(deltaSeconds);
            continue;
        }
        if (ghost.isFrightened() && frightenedSecondsLeft_ > 0.0f) {
            ghost.setFrightenedTimeLeft(frightenedSecondsLeft_);
        } else if (ghost.isFrightened() && frightenedSecondsLeft_ <= 0.0f) {
            ghost.setFrightenedTimeLeft(0.0f);
        }
        ghost.updateTarget(playerPosition, playerDirection, blinkyPosition);
        ghost.setSpeed(speedFor(ghost));
        ghost.update(deltaSeconds);
    }
}

void Game::releaseGhostsIfReady() {
    if (ghosts_.empty()) {
        return;
    }
    const int pellets = player_.pelletsEaten();
    for (Ghost& ghost : ghosts_) {
        if (ghost.isReleased() || ghost.isEyes()) {
            continue;
        }
        bool shouldRelease = false;
        switch (ghost.id()) {
        case GhostId::Blinky:
            shouldRelease = true;
            break;
        case GhostId::Pinky:
            shouldRelease = pellets >= 30;
            break;
        case GhostId::Inky:
            shouldRelease = pellets >= 60;
            break;
        case GhostId::Clyde:
            shouldRelease = pellets >= 90;
            break;
        }
        if (shouldRelease) {
            ghost.setReleased(true);
        }
    }
}

void Game::collectPellet() {
    const int x = player_.tileX();
    const int y = player_.tileY();
    const Tile tile = maze_.tileAt(x, y);
    if (!hasPellet(tile) && !isFruit(tile)) {
        return;
    }

    const Tile taken = maze_.takePellet(x, y);
    if (taken == Tile::Empty) {
        return;
    }

    const bool powerPellet = isPowerPellet(taken);
    const bool isFruitTaken = isFruit(taken);
    const int points = powerPellet ? kPowerPelletPoints : (isFruitTaken ? kFruitPoints : kPelletPoints);
    score_ += points;
    if (score_ >= nextLifeScore_) {
        ++lives_;
        nextLifeScore_ += 10000;
    }
    if (powerPellet || taken == Tile::Pellet) {
        player_.notePelletEaten(taken);
    }

    if (powerPellet) {
        frightenedSecondsLeft_ = level_.config().frightenedSeconds;
        scheduleSeconds_ = 0.0f;
        globalMode_ = GlobalMode::Scatter;
        for (Ghost& ghost : ghosts_) {
            if (ghost.isEyes()) {
                continue;
            }
            ghost.setMode(GhostMode::Frightened);
            ghost.setFrightenedTimeLeft(frightenedSecondsLeft_);
        }
    }

    Event event;
    if (powerPellet) {
        event.type = EventType::PowerPelletEaten;
    } else if (isFruitTaken) {
        event.type = EventType::FruitEaten;
    } else {
        event.type = EventType::PelletEaten;
    }
    event.x = x;
    event.y = y;
    event.points = points;
    pushEvent(event);
}

void Game::checkCollisions() {
    const int x = player_.tileX();
    const int y = player_.tileY();

    for (Ghost& ghost : ghosts_) {
        if (ghost.isEyes()) {
            continue;
        }
        if (ghost.tileX() != x || ghost.tileY() != y) {
            continue;
        }
        if (ghost.isEdible()) {
            const int points = ghost.pointsValue();
            score_ += points;
            if (score_ >= nextLifeScore_) {
                ++lives_;
                nextLifeScore_ += 10000;
            }
            ghost.eat();
            Event event;
            event.type = EventType::GhostEaten;
            event.x = x;
            event.y = y;
            event.points = points;
            event.value = static_cast<int>(ghost.id());
            pushEvent(event);
            continue;
        }
        killPlayer();
        return;
    }
}

void Game::killPlayer() {
    --lives_;
    frightenedSecondsLeft_ = 0.0f;
    state_ = GameState::PlayerDied;
    stateSeconds_ = 0.0f;

    Event event;
    event.type = EventType::PlayerDied;
    event.x = player_.tileX();
    event.y = player_.tileY();
    event.value = lives_;
    pushEvent(event);
}

void Game::finishLevel() {
    state_ = GameState::LevelComplete;
    stateSeconds_ = 0.0f;

    Event event;
    event.type = EventType::LevelCompleted;
    event.x = levelNumber_;
    event.value = maze_.pelletsTotal();
    pushEvent(event);
}

void Game::endGame() {
    state_ = GameState::GameOver;
    stateSeconds_ = 0.0f;

    Event event;
    event.type = EventType::GameOver;
    event.value = score_;
    pushEvent(event);
}

void Game::pushEvent(Event event) {
    events_.push_back(event);
}

std::vector<Event> Game::drainEvents() {
    std::vector<Event> drained;
    drained.swap(events_);
    return drained;
}

}