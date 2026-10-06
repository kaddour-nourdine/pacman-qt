#include "Fixtures.hpp"
#include "TestFramework.hpp"
#include "pacman/Engine.hpp"

using namespace pac;

PAC_TEST(Engine, runsFixedSteps) {
    Game game(pactest::makeLevel(pactest::gameMap(), __FILE__, __LINE__));
    Engine engine(game);

    CHECK_NEAR(Engine::kStepSeconds, 1.0 / 120.0, 1e-12);
    CHECK_EQ(engine.update(Engine::kStepSeconds * 3.5), 3);
    CHECK_EQ(static_cast<long long>(engine.stepCount()), 3);
    CHECK_NEAR(engine.interpolationAlpha(), 0.5, 1e-9);
    CHECK_NEAR(engine.simulatedSeconds(), Engine::kStepSeconds * 3.0, 1e-9);
}

PAC_TEST(Engine, accumulatesPartialSteps) {
    Game game(pactest::makeLevel(pactest::gameMap(), __FILE__, __LINE__));
    Engine engine(game);

    CHECK_EQ(engine.update(Engine::kStepSeconds * 0.5), 0);
    CHECK_NEAR(engine.interpolationAlpha(), 0.5, 1e-9);
    CHECK_EQ(engine.update(Engine::kStepSeconds * 0.5), 1);
    CHECK_EQ(static_cast<long long>(engine.stepCount()), 1);
    CHECK_NEAR(engine.interpolationAlpha(), 0.0, 1e-9);
}

PAC_TEST(Engine, clampsLongFrames) {
    Game game(pactest::makeLevel(pactest::gameMap(), __FILE__, __LINE__));
    Engine engine(game);
    engine.setMaxFrameSeconds(0.1);

    const int steps = engine.update(30.0);

    CHECK_EQ(steps, 12);
    CHECK_NEAR(engine.interpolationAlpha(), 0.0, 1e-9);
}

PAC_TEST(Engine, honoursTimeScale) {
    Game game(pactest::makeLevel(pactest::gameMap(), __FILE__, __LINE__));
    Engine engine(game);

    engine.setTimeScale(0.0);
    CHECK_EQ(engine.update(1.0), 0);
    CHECK_EQ(static_cast<long long>(engine.stepCount()), 0);

    engine.setTimeScale(2.0);
    CHECK_NEAR(engine.timeScale(), 2.0, 1e-9);
    CHECK_EQ(engine.update(Engine::kStepSeconds), 2);

    engine.setTimeScale(-5.0);
    CHECK_NEAR(engine.timeScale(), 0.0, 1e-9);
    CHECK_EQ(engine.update(1.0), 0);
}

PAC_TEST(Engine, ignoresNonPositiveFrames) {
    Game game(pactest::makeLevel(pactest::gameMap(), __FILE__, __LINE__));
    Engine engine(game);

    CHECK_EQ(engine.update(0.0), 0);
    CHECK_EQ(engine.update(-1.0), 0);
    CHECK_EQ(static_cast<long long>(engine.stepCount()), 0);
}

PAC_TEST(Engine, doesNotAdvanceGameWhileReady) {
    Game game(pactest::makeLevel(pactest::gameMap(), __FILE__, __LINE__));
    Engine engine(game);

    engine.update(5.0);

    CHECK_EQ(game.state(), GameState::Ready);
    CHECK_EQ(game.pelletsRemaining(), 34);
    CHECK_EQ(game.player().tileX(), 2);
    CHECK_EQ(game.player().tileY(), 3);
}

PAC_TEST(Engine, drivesSimulationAtRealTime) {
    Game game(pactest::makeLevel(pactest::gameMap(), __FILE__, __LINE__));
    Engine engine(game);
    game.start();

    const int startX = game.player().tileX();
    int steps = 0;
    while (game.score() == 0 && steps < 1000) {
        steps += engine.update(1.0 / 60.0);
    }

    CHECK(steps > 0);
    CHECK_EQ(game.player().tileX(), startX - 1);
    CHECK_EQ(game.score(), Game::kPelletPoints);
    CHECK_EQ(game.pelletsRemaining(), 33);
}

PAC_TEST(Engine, isReproducible) {
    Game first(pactest::makeLevel(pactest::gameMap(), __FILE__, __LINE__));
    Game second(pactest::makeLevel(pactest::gameMap(), __FILE__, __LINE__));
    Engine engineA(first);
    Engine engineB(second);
    first.setRandomSeed(777);
    second.setRandomSeed(777);
    first.start();
    second.start();

    for (int frame = 0; frame < 120; ++frame) {
        if (frame == 30) {
            first.player().requestDirection(Direction::Up);
            second.player().requestDirection(Direction::Up);
        }
        const double wobble = 1.0 / 60.0 + (frame % 3) * 0.004;
        CHECK_EQ(engineA.update(wobble), engineB.update(wobble));
    }

    CHECK_EQ(first.state(), second.state());
    CHECK_EQ(first.score(), second.score());
    CHECK_EQ(first.lives(), second.lives());
    CHECK_EQ(static_cast<long long>(engineA.stepCount()), static_cast<long long>(engineB.stepCount()));
    CHECK(nearlyEquals(first.player().position(), second.player().position(), 1e-6f));
}