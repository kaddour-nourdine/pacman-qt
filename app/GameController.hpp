#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <vector>

#include "AppSettings.hpp"
#include "pacman/Engine.hpp"
#include "pacman/Game.hpp"
#include "pacman/LevelManager.hpp"
#include "pacman/Vec2.hpp"

class GameController : public QObject {
    Q_OBJECT

    Q_PROPERTY(int score READ score NOTIFY statsChanged)
    Q_PROPERTY(int highScore READ highScore NOTIFY highScoreChanged)
    Q_PROPERTY(int lives READ lives NOTIFY statsChanged)
    Q_PROPERTY(int levelNumber READ levelNumber NOTIFY statsChanged)
    Q_PROPERTY(QString levelName READ levelName NOTIFY statsChanged)
    Q_PROPERTY(int pelletsRemaining READ pelletsRemaining NOTIFY statsChanged)
    Q_PROPERTY(QString stateName READ stateName NOTIFY stateChanged)
    Q_PROPERTY(bool paused READ paused NOTIFY pausedChanged)
    Q_PROPERTY(QString mazeData READ mazeData NOTIFY mazeChanged)
    Q_PROPERTY(QString itemData READ itemData NOTIFY itemsChanged)
    Q_PROPERTY(int mazeCols READ mazeCols NOTIFY mazeChanged)
    Q_PROPERTY(int mazeRows READ mazeRows NOTIFY mazeChanged)
    Q_PROPERTY(double playerX READ playerX NOTIFY actorsChanged)
    Q_PROPERTY(double playerY READ playerY NOTIFY actorsChanged)
    Q_PROPERTY(double playerMouth READ playerMouth NOTIFY actorsChanged)
    Q_PROPERTY(int playerDirection READ playerDirection NOTIFY actorsChanged)
    Q_PROPERTY(QVariantList ghosts READ ghosts NOTIFY actorsChanged)
    Q_PROPERTY(bool suspended READ suspended NOTIFY suspendedChanged)
    Q_PROPERTY(AppSettings* settings READ settings CONSTANT)

public:
    explicit GameController(QObject* parent = nullptr);

    Q_INVOKABLE void start();
    Q_INVOKABLE void reset();
    Q_INVOKABLE void togglePause();
    Q_INVOKABLE void requestDirection(int direction);

    int score() const { return game_.score(); }
    int highScore() const { return highScore_; }
    int lives() const { return game_.lives(); }
    int levelNumber() const { return game_.levelNumber(); }
    QString levelName() const;
    int pelletsRemaining() const { return game_.pelletsRemaining(); }
    QString stateName() const;
    bool paused() const { return paused_; }

    // True while the application is in the background (Android) or the
    // window is not interactive: the model is frozen and the tick timer is
    // stopped, so returning to the game does not replay the missing time.
    bool suspended() const { return suspended_; }
    void setSuspended(bool suspended);

    // Persistent user settings (sound, slow mode). Owned by the controller
    // so that QML can reach them as `game.settings`.
    AppSettings* settings() { return &settings_; }

    QString mazeData() const { return mazeData_; }
    QString itemData() const { return itemData_; }
    int mazeCols() const { return game_.maze().cols(); }
    int mazeRows() const { return game_.maze().rows(); }

    double playerX() const;
    double playerY() const;
    double playerMouth() const;
    int playerDirection() const;
    QVariantList ghosts() const;

signals:
    void statsChanged();
    void highScoreChanged();
    void stateChanged();
    void pausedChanged();
    void suspendedChanged();
    void mazeChanged();
    void itemsChanged();
    void actorsChanged();

    void levelStarted(int levelNumber);
    void pelletEaten(int x, int y);
    void powerPelletEaten(int x, int y);
    void fruitEaten(int points, int x, int y);
    void ghostEaten(int points, int x, int y);
    void ghostRespawned();
    void playerDied();
    void levelCompleted();
    void gameOver();

private:
    void tick();
    void drainModelEvents();
    void syncMaze();
    void syncStats();
    void syncState();
    void capturePositions();
    void setPaused(bool paused);
    void applyTimeScale();
    void onApplicationStateChanged(int state);
    double interpolateX(double previous, double current) const;

    static constexpr const char* kHighScoreKey = "highScore";
    // "Slow mode" runs the model at 80% speed: the same levels, but with
    // more time to pick a direction on a touch screen.
    static constexpr double kSlowFactor = 0.8;

    pac::LevelManager levels_;
    pac::Game game_;
    pac::Engine engine_;
    AppSettings settings_;
    QTimer timer_;
    QElapsedTimer frameClock_;

    // Hitch detector state (see tick()).
    QElapsedTimer perfPrevious_;
    QElapsedTimer perfClock_;
    qint64 perfPreviousCpuMs_ = -1;

    pac::Vec2 previousPlayer_;
    std::vector<pac::Vec2> previousGhosts_;

    QString mazeData_;
    QString itemData_;
    QString levelName_;
    QString stateName_ = QStringLiteral("ready");
    int score_ = 0;
    int lives_ = 0;
    int levelNumber_ = 0;
    int pelletsRemaining_ = -1;
    int highScore_ = 0;
    bool paused_ = false;
    bool suspended_ = false;

public:
    int directionForTest() const { return static_cast<int>(game_.player().requestedDirection()); }

    // Copies the levels embedded as Qt resources (":/levels") into the writable
    // app data directory and returns it; empty on failure. `core` reads level
    // files with std::ifstream, which cannot see Qt resources, so the view
    // materialises them once per launch (a few KB).
    static QString materializeBundledLevels();
};
