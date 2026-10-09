#include <vector>

#include "Fixtures.hpp"
#include "TestFramework.hpp"
#include "pacman/Engine.hpp"
#include "pacman/Game.hpp"
#include "pacman/LevelManager.hpp"

using namespace pac;

namespace {

bool hasEvent(const std::vector<Event>& events, EventType type) {
    for (const Event& event : events) {
        if (event.type == type) {
            return true;
        }
    }
    return false;
}

int pelletTilesLeft(const Maze& maze) {
    int count = 0;
    for (int y = 0; y < maze.rows(); ++y) {
        for (int x = 0; x < maze.cols(); ++x) {
            if (hasPellet(maze.tileAt(x, y))) {
                ++count;
            }
        }
    }
    return count;
}

void collectAllPellets(Game& game) {
    for (int y = 0; y < game.maze().rows(); ++y) {
        for (int x = 0; x < game.maze().cols(); ++x) {
            if (!hasPellet(game.maze().tileAt(x, y))) {
                continue;
            }
            game.player().setPosition(tileCenterOf(x, y));
            game.step(0.0001f);
        }
    }
}

}  // namespace

PAC_TEST(Game, startsReadyAndFrozen) {
    Game game(pactest::makeLevel(pactest::gameMap(), __FILE__, __LINE__));
    CHECK_EQ(game.state(), GameState::Ready);
    CHECK(!game.isRunning());
    CHECK_EQ(game.score(), 0);
    CHECK_EQ(game.lives(), 3);
    CHECK_EQ(game.levelNumber(), 1);
    CHECK_EQ(game.pelletsRemaining(), 34);
    CHECK_EQ(game.player().tileX(), 2);
    CHECK_EQ(game.player().tileY(), 3);

    for (int i = 0; i < 60; ++i) {
        game.step(0.1f);
    }
    CHECK_EQ(game.player().tileX(), 2);
    CHECK_EQ(game.player().tileY(), 3);
    CHECK_EQ(game.pelletsRemaining(), 34);
}

PAC_TEST(Game, collectsPelletsWhileRunning) {
    Game game(pactest::makeLevel(pactest::gameMap(), __FILE__, __LINE__));
    game.start();
    CHECK(game.isRunning());

    game.player().setPosition(tileCenterOf(1, 1));
    game.step(0.01f);

    CHECK_EQ(game.pelletsRemaining(), 33);
    CHECK_EQ(game.score(), Game::kPelletPoints);
    CHECK_EQ(game.player().pelletsEaten(), 1);

    const std::vector<Event> events = game.drainEvents();
    CHECK(hasEvent(events, EventType::LevelStarted));
    CHECK(hasEvent(events, EventType::PelletEaten));
    CHECK(game.drainEvents().empty());
}

PAC_TEST(Game, powerPelletFrightensGhosts) {
    Game game(pactest::makeLevel(pactest::gameMap(), __FILE__, __LINE__));
    game.start();

    game.player().setPosition(tileCenterOf(10, 5));
    game.step(0.001f);

    CHECK(game.isFrightened());
    CHECK(game.frightenedSecondsLeft() > 0.0f);
    CHECK_EQ(game.score(), Game::kPowerPelletPoints);
    CHECK_EQ(game.player().powerPelletsEaten(), 1);
    for (const Ghost& ghost : game.ghosts()) {
        CHECK(ghost.isFrightened());
    }
    CHECK(hasEvent(game.drainEvents(), EventType::PowerPelletEaten));
}

PAC_TEST(Game, frightenedEndsAndReturnsToSchedule) {
    Game game(pactest::makeLevel(pactest::modeMap(), __FILE__, __LINE__));
    game.start();

    game.step(0.5f);
    CHECK_EQ(game.globalMode(), GlobalMode::Scatter);
    game.step(0.5f);
    CHECK_EQ(game.globalMode(), GlobalMode::Chase);

    game.player().setPosition(tileCenterOf(10, 5));
    game.step(0.001f);
    CHECK(game.isFrightened());
    CHECK_EQ(game.globalMode(), GlobalMode::Scatter);

    game.step(0.5f);
    CHECK(game.isFrightened());
    CHECK_EQ(game.globalMode(), GlobalMode::Scatter);

    game.step(0.6f);
    CHECK(!game.isFrightened());
    CHECK_EQ(game.globalMode(), GlobalMode::Scatter);

    game.step(0.5f);
    CHECK_EQ(game.globalMode(), GlobalMode::Scatter);

    game.step(0.5f);
    CHECK_EQ(game.globalMode(), GlobalMode::Chase);
}

PAC_TEST(Game, eatsFrightenedGhost) {
    Game game(pactest::makeLevel(pactest::gameMap(), __FILE__, __LINE__));
    game.start();

    game.player().setPosition(tileCenterOf(10, 5));
    game.step(0.001f);

    Ghost& blinky = game.ghosts()[0];
    blinky.setPosition(tileCenterOf(10, 5));
    game.step(0.001f);

    CHECK(blinky.isEyes());
    CHECK_EQ(game.score(), Game::kPowerPelletPoints + 200);
    CHECK(hasEvent(game.drainEvents(), EventType::GhostEaten));
}

PAC_TEST(Game, eyesRespawnAtHome) {
    Game game(pactest::makeLevel(pactest::gameMap(), __FILE__, __LINE__));
    game.start();

    Ghost& blinky = game.ghosts()[0];
    CHECK_EQ(blinky.spawnX(), 4);
    CHECK_EQ(blinky.spawnY(), 3);

    blinky.eat();
    CHECK(blinky.isEyes());
    game.step(0.001f);

    CHECK(!blinky.isEyes());
    CHECK_EQ(blinky.tileX(), 4);
    CHECK_EQ(blinky.tileY(), 3);
    CHECK(hasEvent(game.drainEvents(), EventType::GhostRespawned));
}

PAC_TEST(Game, collisionKillsPlayer) {
    Game game(pactest::makeLevel(pactest::gameMap(), __FILE__, __LINE__));
    game.start();

    game.player().setPosition(tileCenterOf(4, 3));
    game.step(0.001f);

    CHECK_EQ(game.state(), GameState::PlayerDied);
    CHECK_EQ(game.lives(), 2);
    CHECK(hasEvent(game.drainEvents(), EventType::PlayerDied));

    game.step(Game::kDeathPauseSeconds + 0.01f);
    CHECK_EQ(game.state(), GameState::Ready);
    CHECK_EQ(game.player().tileX(), 2);
}

PAC_TEST(Game, gameOverAfterLosingAllLives) {
    Game game(pactest::makeLevel(pactest::gameMap(), __FILE__, __LINE__));
    game.setRandomSeed(5);

    for (int life = 0; life < 3; ++life) {
        game.start();
        Ghost& blinky = game.ghosts()[0];
        blinky.setPosition(tileCenterOf(game.player().tileX(), game.player().tileY()));
        game.step(0.001f);
        CHECK_EQ(game.state(), GameState::PlayerDied);
        CHECK_EQ(game.lives(), 2 - life);
        game.step(Game::kDeathPauseSeconds + 0.01f);
    }

    CHECK_EQ(game.state(), GameState::GameOver);
    CHECK(game.isOver());
    CHECK(hasEvent(game.drainEvents(), EventType::GameOver));
}

PAC_TEST(Game, completesLevelAndAdvances) {
    Game game(pactest::makeLevel(pactest::gameMap(), __FILE__, __LINE__));
    game.start();

    CHECK_EQ(pelletTilesLeft(game.maze()), 34);
    collectAllPellets(game);

    CHECK_EQ(game.pelletsRemaining(), 0);
    CHECK_EQ(game.state(), GameState::LevelComplete);
    CHECK_EQ(game.score(), 33 * Game::kPelletPoints + Game::kPowerPelletPoints);
    CHECK(hasEvent(game.drainEvents(), EventType::LevelCompleted));

    game.step(Game::kLevelCompletePauseSeconds + 0.01f);
    CHECK_EQ(game.state(), GameState::Ready);
    CHECK_EQ(game.levelNumber(), 2);
    CHECK_EQ(game.pelletsRemaining(), 34);
}

PAC_TEST(Game, resetRestartsFromBeginning) {
    Game game(pactest::makeLevel(pactest::gameMap(), __FILE__, __LINE__));
    game.start();
    game.player().setPosition(tileCenterOf(1, 1));
    game.step(0.01f);
    game.player().setPosition(tileCenterOf(4, 3));
    game.step(0.001f);
    CHECK_EQ(game.lives(), 2);

    game.reset();
    CHECK_EQ(game.state(), GameState::Ready);
    CHECK_EQ(game.score(), 0);
    CHECK_EQ(game.lives(), 3);
    CHECK_EQ(game.levelNumber(), 1);
    CHECK_EQ(game.pelletsRemaining(), 34);
    CHECK_EQ(game.player().pelletsEaten(), 0);
}

PAC_TEST(Game, playsLevelsFromTheList) {
    LevelManager manager;
    CHECK(manager.add(pactest::makeLevel(pactest::firstRoomMap(), __FILE__, __LINE__)));
    CHECK(manager.add(pactest::makeLevel(pactest::secondRoomMap(), __FILE__, __LINE__)));

    Game game(manager);
    game.start();
    CHECK_EQ(game.levelNumber(), 1);
    CHECK(game.levelName() == "First Room");
    CHECK_EQ(game.pelletsRemaining(), 15);

    game.ghosts()[0].setPosition(game.player().position());
    game.step(0.001f);
    CHECK_EQ(game.state(), GameState::PlayerDied);
    CHECK_EQ(game.lives(), 2);
    game.step(Game::kDeathPauseSeconds + 0.01f);
    CHECK_EQ(game.state(), GameState::Ready);
    game.start();

    collectAllPellets(game);
    CHECK_EQ(game.state(), GameState::LevelComplete);
    CHECK_EQ(game.score(), 14 * Game::kPelletPoints + Game::kPowerPelletPoints);

    game.step(Game::kLevelCompletePauseSeconds + 0.01f);
    CHECK_EQ(game.levelNumber(), 2);
    CHECK(game.levelName() == "Second Room");
    CHECK_EQ(game.lives(), 2);
    CHECK_EQ(game.pelletsRemaining(), 12);
    CHECK_EQ(game.player().pelletsEaten(), 0);
    CHECK_EQ(game.score(), 14 * Game::kPelletPoints + Game::kPowerPelletPoints);

    game.start();
    collectAllPellets(game);
    CHECK_EQ(game.state(), GameState::LevelComplete);
    game.step(Game::kLevelCompletePauseSeconds + 0.01f);
    CHECK_EQ(game.levelNumber(), 3);
    CHECK(game.levelName() == "First Room");
    CHECK_EQ(game.pelletsRemaining(), 15);
    CHECK_EQ(game.score(), 25 * Game::kPelletPoints + 2 * Game::kPowerPelletPoints);
}

PAC_TEST(Game, resetReturnsToFirstLevelOfTheList) {
    LevelManager manager;
    CHECK(manager.add(pactest::makeLevel(pactest::firstRoomMap(), __FILE__, __LINE__)));
    CHECK(manager.add(pactest::makeLevel(pactest::secondRoomMap(), __FILE__, __LINE__)));

    Game game(manager);
    game.start();
    collectAllPellets(game);
    game.step(Game::kLevelCompletePauseSeconds + 0.01f);
    CHECK_EQ(game.levelNumber(), 2);
    CHECK(game.levelName() == "Second Room");

    game.reset();
    CHECK_EQ(game.state(), GameState::Ready);
    CHECK_EQ(game.levelNumber(), 1);
    CHECK(game.levelName() == "First Room");
    CHECK_EQ(game.score(), 0);
    CHECK_EQ(game.lives(), 3);
    CHECK_EQ(game.pelletsRemaining(), 15);
}

PAC_TEST(Game, startsOverWhenTheLevelListIsEmpty) {
    const LevelManager empty;
    Game game{empty};
    CHECK(game.isOver());
    CHECK(game.levelName().empty());
    CHECK_EQ(game.levelNumber(), 1);
    CHECK_EQ(game.score(), 0);
    CHECK(hasEvent(game.drainEvents(), EventType::GameOver));

    game.start();
    game.step(1.0f);
    CHECK(game.isOver());

    game.reset();
    CHECK(game.isOver());
    CHECK(hasEvent(game.drainEvents(), EventType::GameOver));
}

PAC_TEST(Game, ghostsAreReproducibleForSameSeed) {
    Game first(pactest::makeLevel(pactest::gameMap(), __FILE__, __LINE__));
    Game second(pactest::makeLevel(pactest::gameMap(), __FILE__, __LINE__));
    first.setRandomSeed(20240);
    second.setRandomSeed(20240);
    first.start();
    second.start();

    for (int i = 0; i < 400; ++i) {
        if (i == 40) {
            first.player().requestDirection(Direction::Left);
            second.player().requestDirection(Direction::Left);
        }
        first.step(1.0f / 60.0f);
        second.step(1.0f / 60.0f);
    }

    CHECK_EQ(first.state(), second.state());
    CHECK_EQ(first.score(), second.score());
    CHECK_EQ(first.lives(), second.lives());
    CHECK(nearlyEquals(first.player().position(), second.player().position(), 1e-5f));
    for (std::size_t i = 0; i < first.ghosts().size(); ++i) {
        CHECK(nearlyEquals(first.ghosts()[i].position(), second.ghosts()[i].position(), 1e-5f));
        CHECK_EQ(first.ghosts()[i].mode(), second.ghosts()[i].mode());
    }
}

PAC_TEST(Game, playsRealLevelEndToEnd) {
    Level level;
    std::string error;
    if (!Level::loadFile(std::string(PACMAN_LEVELS_DIR) + "/level1.txt", level, &error)) {
        pactest::reportFailure(__FILE__, __LINE__, "level1.txt failed to load: " + error);
        return;
    }

    Game game(std::move(level));
    Engine engine(game);
    game.start();

    CHECK_EQ(game.pelletsRemaining(), 159);
    CHECK_EQ(game.ghosts()[1].spawnX(), 9);
    CHECK_EQ(game.ghosts()[1].spawnY(), 7);

    for (int frame = 0; frame < 600; ++frame) {
        if (frame == 120) {
            game.player().requestDirection(Direction::Down);
        }
        engine.update(1.0 / 60.0);
    }

    CHECK(game.score() > 0);
    CHECK(game.pelletsRemaining() < 159);
    CHECK(game.player().pelletsEaten() > 0);
    CHECK(game.ghosts()[1].tileX() != 9 || game.ghosts()[1].tileY() != 7);
    CHECK(!game.isOver() || game.lives() == 0);
}

PAC_TEST(Game, stateNames) {
    CHECK_EQ(toString(GameState::Ready)[0], 'r');
    CHECK_EQ(toString(GlobalMode::Scatter)[0], 's');
    CHECK_EQ(toString(GlobalMode::Chase)[0], 'c');
}

PAC_TEST(ModeSchedule, alternatesPhases) {
    const ModeSchedule schedule;
    CHECK(!schedule.usesCustomSchedule());
    CHECK_EQ(schedule.phaseCount(), 8);
    CHECK_EQ(schedule.modeAt(0.0f), GlobalMode::Scatter);
    CHECK_EQ(schedule.modeAt(6.9f), GlobalMode::Scatter);
    CHECK_EQ(schedule.modeAt(7.0f), GlobalMode::Chase);
    CHECK_EQ(schedule.modeAt(26.9f), GlobalMode::Chase);
    CHECK_EQ(schedule.modeAt(27.0f), GlobalMode::Scatter);
    CHECK_EQ(schedule.modeAt(10000.0f), GlobalMode::Chase);
}

PAC_TEST(ModeSchedule, honoursCustomDurations) {
    const ModeSchedule schedule(std::vector<float>{1.0f, 2.0f});
    CHECK(schedule.usesCustomSchedule());
    CHECK_EQ(schedule.phaseCount(), 2);
    CHECK_EQ(schedule.modeAt(0.5f), GlobalMode::Scatter);
    CHECK_EQ(schedule.modeAt(1.5f), GlobalMode::Chase);
    CHECK_EQ(schedule.modeAt(3.5f), GlobalMode::Chase);
    CHECK_NEAR(schedule.phaseDuration(1), 2.0, 1e-6);
}

PAC_TEST(Game, soakTestInvariants) {
    Level level;
    std::string error;
    const std::string path = std::string(PACMAN_LEVELS_DIR) + "/level1.txt";
    CHECK(Level::loadFile(path, level, &error));

    Game game(level);
    for (Ghost& ghost : game.ghosts()) {
        ghost.setReleased(true);
    }
    Engine engine(game);
    engine.setTimeScale(10.0f);

    const int totalPellets = game.pelletsRemaining();
    const int totalPowerPellets = 4;
    const int maxScore = totalPellets * 10 + totalPowerPellets * 50 + 4 * 1600;

    for (int step = 0; step < 200000; ++step) {
        engine.update(1.0f / 120.0f);
        if (game.isOver() || game.state() == GameState::LevelComplete) {
            break;
        }
        if (game.player().tileX() >= 0 && game.player().tileX() < game.maze().cols() &&
            game.player().tileY() >= 0 && game.player().tileY() < game.maze().rows()) {
            const Tile pt = game.maze().tileAt(game.player().tileX(), game.player().tileY());
            CHECK(pt != Tile::Wall);
            CHECK(pt != Tile::Door);
        }
        for (const auto& ghost : game.ghosts()) {
            const int gx = ghost.tileX();
            const int gy = ghost.tileY();
            if (gx < 0 || gx >= game.maze().cols() || gy < 0 || gy >= game.maze().rows()) {
                CHECK(false);
                continue;
            }
            CHECK(game.maze().tileAt(gx, gy) != Tile::Wall);
        }
        CHECK(game.score() >= 0);
        if (maxScore > 0) {
            CHECK(game.score() <= maxScore);
        }
    }
}

PAC_TEST(Game, ghostsStayOnWalkableTilesAfterMidTileReversal) {
    Level level;
    std::string error;
    const std::string path = std::string(PACMAN_LEVELS_DIR) + "/level1.txt";
    CHECK(Level::loadFile(path, level, &error));

    Game game(std::move(level));
    game.setRandomSeed(12345);
    Engine engine(game);
    game.start();
    for (Ghost& ghost : game.ghosts()) {
        ghost.setReleased(true);
    }

    for (int step = 0; step < 120; ++step) {
        engine.update(1.0f / 120.0f);
    }

    game.player().setPosition(tileCenterOf(1, 3));

    for (int step = 0; step < 40000; ++step) {
        engine.update(1.0f / 120.0f);
        if (game.isOver()) {
            break;
        }
        for (const Ghost& ghost : game.ghosts()) {
            const int gx = ghost.tileX();
            const int gy = ghost.tileY();
            if (gx < 0 || gx >= game.maze().cols() || gy < 0 || gy >= game.maze().rows()) {
                CHECK(false);
                continue;
            }
            CHECK(game.maze().tileAt(gx, gy) != Tile::Wall);
        }
    }
}
