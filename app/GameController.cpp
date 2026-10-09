#include "GameController.hpp"

#include <QDebug>
#include <QSettings>
#include <QVariantMap>

#include <cmath>
#include <string>

namespace {

pac::LevelManager loadLevels() {
    pac::LevelManager levels;
    std::string error;
    if (!levels.loadDirectory(PACMAN_LEVELS_DIR, &error)) {
        qWarning() << "pacman: cannot load levels from" << PACMAN_LEVELS_DIR << ":" << error.c_str();
    }
    if (levels.isEmpty()) {
        qWarning() << "pacman: no levels found in" << PACMAN_LEVELS_DIR;
    }
    return levels;
}

}

GameController::GameController(QObject* parent)
    : QObject(parent),
      levels_(loadLevels()),
      game_(levels_),
      engine_(game_) {
    highScore_ = QSettings().value(kHighScoreKey, 0).toInt();
    capturePositions();
    frameClock_.start();

    connect(&timer_, &QTimer::timeout, this, &GameController::tick);
    timer_.setTimerType(Qt::PreciseTimer);
    timer_.setInterval(16);
    timer_.start();

    syncMaze();
    syncStats();
    syncState();
}

void GameController::start() {
    if (game_.state() == pac::GameState::GameOver) {
        setPaused(false);
        game_.reset();
    }
    if (game_.state() == pac::GameState::Ready) {
        game_.start();
    }
}

void GameController::reset() {
    setPaused(false);
    game_.reset();
}

void GameController::togglePause() {
    if (game_.state() == pac::GameState::Running) {
        setPaused(!paused_);
    }
}

void GameController::requestDirection(int direction) {
    if (direction < 0 || direction > 4) {
        return;
    }
    if (game_.state() == pac::GameState::GameOver) {
        return;
    }
    if (game_.state() == pac::GameState::Ready) {
        game_.start();
    }
    game_.player().requestDirection(static_cast<pac::Direction>(direction));
}

QString GameController::levelName() const {
    return levelName_;
}

QString GameController::stateName() const {
    return stateName_;
}

double GameController::playerX() const {
    return interpolateX(previousPlayer_.x, game_.player().position().x);
}

double GameController::playerY() const {
    const double previous = previousPlayer_.y;
    const double current = game_.player().position().y;
    const double alpha = engine_.interpolationAlpha();
    return previous + (current - previous) * alpha;
}

double GameController::playerMouth() const {
    return game_.player().animationPhase();
}

int GameController::playerDirection() const {
    return static_cast<int>(game_.player().direction());
}

QVariantList GameController::ghosts() const {
    QVariantList list;
    const std::vector<pac::Ghost>& modelGhosts = game_.ghosts();
    const double alpha = engine_.interpolationAlpha();
    list.reserve(static_cast<int>(modelGhosts.size()));
    for (std::size_t i = 0; i < modelGhosts.size(); ++i) {
        const pac::Ghost& ghost = modelGhosts[i];
        const pac::Vec2 current = ghost.position();
        const pac::Vec2 previous = i < previousGhosts_.size() ? previousGhosts_[i] : current;
        QVariantMap entry;
        entry.insert(QStringLiteral("x"), interpolateX(previous.x, current.x));
        entry.insert(QStringLiteral("y"), previous.y + (current.y - previous.y) * alpha);
        entry.insert(QStringLiteral("id"), static_cast<int>(ghost.id()));
        entry.insert(QStringLiteral("eyes"), ghost.isEyes());
        entry.insert(QStringLiteral("edible"), ghost.isEdible());
        entry.insert(QStringLiteral("flashing"), ghost.isFrightenedFlashing());
        list.append(entry);
    }
    return list;
}

void GameController::tick() {
    capturePositions();
    const double frameSeconds = static_cast<double>(frameClock_.nsecsElapsed()) * 1e-9;
    frameClock_.restart();
    engine_.update(frameSeconds);
    drainModelEvents();
    syncMaze();
    syncStats();
    syncState();
    emit actorsChanged();
}

void GameController::drainModelEvents() {
    for (const pac::Event& event : game_.drainEvents()) {
        switch (event.type) {
        case pac::EventType::LevelStarted:
            emit levelStarted(event.value);
            break;
        case pac::EventType::PelletEaten:
            emit pelletEaten(event.x, event.y);
            break;
        case pac::EventType::PowerPelletEaten:
            emit powerPelletEaten(event.x, event.y);
            break;
        case pac::EventType::FruitEaten:
            emit fruitEaten(event.points, event.x, event.y);
            break;
        case pac::EventType::GhostEaten:
            emit ghostEaten(event.points, event.x, event.y);
            break;
        case pac::EventType::GhostRespawned:
            emit ghostRespawned();
            break;
        case pac::EventType::PlayerDied:
            emit playerDied();
            break;
        case pac::EventType::LevelCompleted:
            emit levelCompleted();
            break;
        case pac::EventType::GameOver:
            if (score_ > highScore_) {
                highScore_ = score_;
                QSettings().setValue(kHighScoreKey, highScore_);
                emit highScoreChanged();
            }
            emit gameOver();
            break;
        }
    }
}

void GameController::syncMaze() {
    const pac::Maze& maze = game_.maze();
    const int cols = maze.cols();
    const int rows = maze.rows();
    QString walls;
    QString items;
    walls.reserve(rows * cols);
    items.reserve(rows * cols);
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            const pac::Tile tile = maze.tileAt(x, y);
            switch (tile) {
            case pac::Tile::Wall:
                walls.append(QLatin1Char('#'));
                break;
            case pac::Tile::Door:
                walls.append(QLatin1Char('-'));
                break;
            case pac::Tile::Tunnel:
                walls.append(QLatin1Char('_'));
                break;
            default:
                walls.append(QLatin1Char(' '));
                break;
            }
            switch (tile) {
            case pac::Tile::Pellet:
                items.append(QLatin1Char('.'));
                break;
            case pac::Tile::PowerPellet:
                items.append(QLatin1Char('o'));
                break;
            case pac::Tile::Fruit:
                items.append(QLatin1Char('F'));
                break;
            default:
                items.append(QLatin1Char(' '));
                break;
            }
        }
    }
    if (walls != mazeData_) {
        mazeData_ = walls;
        emit mazeChanged();
    }
    if (items != itemData_) {
        itemData_ = items;
        emit itemsChanged();
    }
}

void GameController::syncStats() {
    bool changed = false;
    if (score_ != game_.score()) {
        score_ = game_.score();
        changed = true;
    }
    if (lives_ != game_.lives()) {
        lives_ = game_.lives();
        changed = true;
    }
    if (levelNumber_ != game_.levelNumber()) {
        levelNumber_ = game_.levelNumber();
        changed = true;
    }
    if (pelletsRemaining_ != game_.pelletsRemaining()) {
        pelletsRemaining_ = game_.pelletsRemaining();
        changed = true;
    }
    const QString name = QString::fromStdString(game_.levelName());
    if (levelName_ != name) {
        levelName_ = name;
        changed = true;
    }
    if (changed) {
        emit statsChanged();
    }
    if (game_.score() > highScore_) {
        highScore_ = game_.score();
        QSettings().setValue(kHighScoreKey, highScore_);
        emit highScoreChanged();
    }
}

void GameController::syncState() {
    QString name;
    switch (game_.state()) {
    case pac::GameState::Ready:
        name = QStringLiteral("ready");
        break;
    case pac::GameState::Running:
        name = QStringLiteral("running");
        break;
    case pac::GameState::PlayerDied:
        name = QStringLiteral("died");
        break;
    case pac::GameState::LevelComplete:
        name = QStringLiteral("levelComplete");
        break;
    case pac::GameState::GameOver:
        name = QStringLiteral("gameOver");
        break;
    }
    if (name != stateName_) {
        stateName_ = name;
        emit stateChanged();
    }
}

void GameController::capturePositions() {
    previousPlayer_ = game_.player().position();
    previousGhosts_.clear();
    previousGhosts_.reserve(game_.ghosts().size());
    for (const pac::Ghost& ghost : game_.ghosts()) {
        previousGhosts_.push_back(ghost.position());
    }
}

void GameController::setPaused(bool paused) {
    if (paused_ == paused) {
        return;
    }
    paused_ = paused;
    engine_.setTimeScale(paused_ ? 0.0 : 1.0);
    emit pausedChanged();
}

double GameController::interpolateX(double previous, double current) const {
    const double halfWidth = static_cast<double>(game_.maze().cols()) * 0.5;
    if (std::fabs(current - previous) > halfWidth) {
        return current;
    }
    const double alpha = engine_.interpolationAlpha();
    return previous + (current - previous) * alpha;
}
