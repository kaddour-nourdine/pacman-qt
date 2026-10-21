#pragma once

#include <cmath>

namespace pac {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

constexpr Vec2 operator+(Vec2 a, Vec2 b) {
    return {a.x + b.x, a.y + b.y};
}

constexpr Vec2 operator-(Vec2 a, Vec2 b) {
    return {a.x - b.x, a.y - b.y};
}

constexpr Vec2 operator-(Vec2 a) {
    return {-a.x, -a.y};
}

constexpr Vec2 operator*(Vec2 a, float scale) {
    return {a.x * scale, a.y * scale};
}

constexpr Vec2 operator*(float scale, Vec2 a) {
    return a * scale;
}

constexpr Vec2 operator/(Vec2 a, float scale) {
    return {a.x / scale, a.y / scale};
}

constexpr bool operator==(Vec2 a, Vec2 b) {
    return a.x == b.x && a.y == b.y;
}

constexpr bool operator!=(Vec2 a, Vec2 b) {
    return !(a == b);
}

inline float lengthSquared(Vec2 v) {
    return v.x * v.x + v.y * v.y;
}

inline float length(Vec2 v) {
    return std::sqrt(v.x * v.x + v.y * v.y);
}

inline float distance(Vec2 a, Vec2 b) {
    return length(b - a);
}

inline bool nearlyEquals(Vec2 a, Vec2 b, float epsilon = 1e-4f) {
    return std::fabs(a.x - b.x) <= epsilon && std::fabs(a.y - b.y) <= epsilon;
}

constexpr Vec2 tileCenterOf(int x, int y) {
    return {static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f};
}

constexpr int tileIndexOf(float value) {
    return static_cast<int>(std::floor(value));
}

} // namespace pac