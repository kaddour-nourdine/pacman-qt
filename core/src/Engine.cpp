#include "pacman/Engine.hpp"

namespace pac {

Engine::Engine(Game& game) : game_(game) {}

void Engine::setTimeScale(double timeScale) {
    timeScale_ = timeScale < 0.0 ? 0.0 : timeScale;
}

void Engine::setMaxFrameSeconds(double seconds) {
    maxFrameSeconds_ = seconds > kStepSeconds ? seconds : kStepSeconds;
}

int Engine::update(double frameSeconds) {
    if (frameSeconds <= 0.0 || timeScale_ <= 0.0) {
        return 0;
    }

    double scaled = frameSeconds * timeScale_;
    if (scaled > maxFrameSeconds_) {
        scaled = maxFrameSeconds_;
    }

    accumulator_ += scaled;

    int steps = 0;
    while (accumulator_ >= kStepSeconds) {
        game_.step(static_cast<float>(kStepSeconds));
        accumulator_ -= kStepSeconds;
        simulatedSeconds_ += kStepSeconds;
        ++stepCount_;
        ++steps;
    }
    return steps;
}

} // namespace pac