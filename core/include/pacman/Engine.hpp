#pragma once

#include <cstdint>

#include "pacman/Game.hpp"

namespace pac {

class Engine {
  public:
    static constexpr double kStepSeconds = 1.0 / 120.0;

    explicit Engine(Game& game);

    void setTimeScale(double timeScale);
    double timeScale() const { return timeScale_; }
    void setMaxFrameSeconds(double seconds);

    int update(double frameSeconds);

    double interpolationAlpha() const { return accumulator_ / kStepSeconds; }
    double simulatedSeconds() const { return simulatedSeconds_; }
    std::uint64_t stepCount() const { return stepCount_; }

  private:
    Game& game_;
    double accumulator_ = 0.0;
    double timeScale_ = 1.0;
    double maxFrameSeconds_ = 0.25;
    double simulatedSeconds_ = 0.0;
    std::uint64_t stepCount_ = 0;
};

} // namespace pac