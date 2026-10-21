#pragma once

#include <cstdint>

namespace pac {

class Random {
  public:
    explicit Random(std::uint32_t seed = 0x9e3779b9u) : state_(seed == 0u ? 0x1u : seed) {}

    void seed(std::uint32_t value) { state_ = value == 0u ? 0x1u : value; }

    std::uint32_t next() {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return state_;
    }

    float unit() { return static_cast<float>(next() >> 8) / static_cast<float>(1u << 24); }

    int range(int lowInclusive, int highInclusive) {
        if (highInclusive <= lowInclusive) {
            return lowInclusive;
        }
        const std::uint32_t span = static_cast<std::uint32_t>(highInclusive - lowInclusive + 1);
        return lowInclusive + static_cast<int>(next() % span);
    }

  private:
    std::uint32_t state_;
};

} // namespace pac