#include "GameController.hpp"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QVariantMap>

#include <cmath>
#include <string>

#if defined(Q_OS_ANDROID) || defined(Q_OS_LINUX)
#include <sys/resource.h>
#endif

namespace {

// Consumed CPU of the calling thread — distinguishes "UI thread was asleep
// during a tick gap" (≈0) from "busy with invisible work" (≈ gap size).
qint64 threadCpuMs() {
#if defined(Q_OS_ANDROID) || defined(Q_OS_LINUX)
    struct rusage ru {};
    if (::getrusage(RUSAGE_THREAD, &ru) != 0) {
        return -1;
    }
    return (static_cast<qint64>(ru.ru_utime.tv_sec) + ru.ru_stime.tv_sec) * 1000 +
           (ru.ru_utime.tv_usec + ru.ru_stime.tv_usec) / 1000;
#else
    return -1;
#endif
}


pac::LevelManager loadLevels() {
    pac::LevelManager levels;
    std::string error;

    // 1) The source tree: present in dev builds, so day-to-day level edits are
    //    picked up without a rebuild.
    if (levels.loadDirectory(PACMAN_LEVELS_DIR, &error) && !levels.isEmpty()) {
        return levels;
    }

    // 2) The levels embedded in the binary (see app/CMakeLists.txt). This is
    //    the only source that exists on a phone, where PACMAN_LEVELS_DIR is a
    //    build-host path.
    levels.clear();
    const QString bundledDir = GameController::materializeBundledLevels();
    if (!bundledDir.isEmpty() &&
        levels.loadDirectory(bundledDir.toStdString(), &error) && !levels.isEmpty()) {
        qInfo() << "pacman: using levels bundled with the binary ->" << bundledDir;
        return levels;
    }

    qWarning() << "pacman: no levels available; source dir" << PACMAN_LEVELS_DIR
               << "- error:" << QString::fromStdString(error);
    return levels;
}

}

QString GameController::materializeBundledLevels() {
    const QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (root.isEmpty()) {
        qWarning() << "pacman: no writable app data location; cannot unpack bundled levels";
        return QString();
    }

    const QDir source(QStringLiteral(":/levels"));
    const QStringList names = source.entryList({QStringLiteral("*.txt")}, QDir::Files, QDir::Name);
    if (names.isEmpty()) {
        qWarning() << "pacman: no levels embedded in the binary";
        return QString();
    }

    QDir dest(root);
    if (!dest.mkpath(QStringLiteral("levels"))) {
        qWarning() << "pacman: cannot create" << dest.filePath(QStringLiteral("levels"));
        return QString();
    }
    const QString dir = dest.filePath(QStringLiteral("levels"));

    // Rewrite unconditionally: the files are tiny and this keeps an installed
    // app up to date when a new package ships changed levels.
    for (const QString& name : names) {
        QFile in(source.filePath(name));
        QFile out(dir + QLatin1Char('/') + name);
        if (!in.open(QIODevice::ReadOnly)) {
            qWarning() << "pacman: cannot read bundled level" << name;
            continue;
        }
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            qWarning() << "pacman: cannot write level file" << out.fileName();
            continue;
        }
        out.write(in.readAll());
    }
    return dir;
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
    // Hitch detector: logs only when a tick was delayed (>40 ms since the
    // previous one) or does abnormally heavy work (>8 ms), naming the phase
    // responsible. On by design — the lines only appear when something is
    // actually wrong. Statics: diagnostics assume the usual single instance.
    static QElapsedTimer previous;
    static QElapsedTimer perf;
    static qint64 previousCpuMs = -1;
    const qint64 gapMs = previous.isValid() ? previous.elapsed() : 0;
    const qint64 cpuNowMs = threadCpuMs();
    const qint64 gapCpuMs = (previousCpuMs >= 0 && cpuNowMs >= 0) ? cpuNowMs - previousCpuMs : -1;
    previousCpuMs = cpuNowMs;
    previous.restart();
    perf.restart();

    capturePositions();
    const double frameSeconds = static_cast<double>(frameClock_.nsecsElapsed()) * 1e-9;
    frameClock_.restart();
    engine_.update(frameSeconds);
    const qint64 updateMs = perf.elapsed();
    drainModelEvents();
    const qint64 drainMs = perf.elapsed() - updateMs;
    syncMaze();
    syncStats();
    const qint64 syncMs = perf.elapsed() - updateMs - drainMs;
    syncState();
    emit actorsChanged();
    const qint64 totalMs = perf.elapsed();

    if (gapMs >= 40 || totalMs >= 8) {
        qInfo() << "pacman-perf: tick gap" << gapMs << "ms; work" << totalMs
                << "ms (update" << updateMs << "drain" << drainMs << "sync" << syncMs
                << "; ui-cpu" << gapCpuMs << "ms)";
    }
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
