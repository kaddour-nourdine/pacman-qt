#pragma once

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace pactest {

struct TestCase {
    const char* suite;
    const char* name;
    void (*run)();
};

std::vector<TestCase>& registry();
int& failureCount();
void reportFailure(const char* file, int line, const std::string& message);

struct Registrar {
    Registrar(const char* suite, const char* name, void (*run)());
};

void checkTrue(bool value, const char* expression, const char* file, int line);
void checkNear(double actual, double expected, double epsilon, const char* actualExpr,
               const char* expectedExpr, const char* file, int line);

template <typename A, typename B>
void checkEqualTyped(const A& actual, const B& expected, const char* actualExpr,
                     const char* expectedExpr, const char* file, int line) {
    if (static_cast<long long>(actual) == static_cast<long long>(expected)) {
        return;
    }
    char buffer[256];
    std::snprintf(buffer, sizeof(buffer), "%s == %s (%lld != %lld)", actualExpr, expectedExpr,
                  static_cast<long long>(actual), static_cast<long long>(expected));
    reportFailure(file, line, buffer);
}

int runAll(const char* suiteFilter);

} // namespace pactest

#define PAC_TEST(suite, name)                                                                      \
    static void suite##_##name##_body();                                                           \
    static const pactest::Registrar suite##_##name##_registrar(#suite, #name,                      \
                                                               &suite##_##name##_body);            \
    static void suite##_##name##_body()

#define CHECK(expr) pactest::checkTrue(static_cast<bool>(expr), #expr, __FILE__, __LINE__)

#define CHECK_EQ(actual, expected)                                                                 \
    pactest::checkEqualTyped((actual), (expected), #actual, #expected, __FILE__, __LINE__)

#define CHECK_NEAR(actual, expected, epsilon)                                                      \
    pactest::checkNear((actual), (expected), (epsilon), #actual, #expected, __FILE__, __LINE__)
