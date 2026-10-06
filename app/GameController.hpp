#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <vector>

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
    double interpolateX(double previous, double current) const;

    static constexpr const char* kHighScoreKey = "highScore";

    pac::LevelManager levels_;
    pac::Game game_;
    pac::Engine engine_;
    QTimer timer_;
    QElapsedTimer frameClock_;

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
};
