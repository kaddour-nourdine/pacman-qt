#include "TestFramework.hpp"

namespace pactest {

std::vector<TestCase>& registry() {
    static std::vector<TestCase> cases;
    return cases;
}

int& failureCount() {
    static int count = 0;
    return count;
}

Registrar::Registrar(const char* suite, const char* name, void (*run)()) {
    registry().push_back(TestCase{suite, name, run});
}

void reportFailure(const char* file, int line, const std::string& message) {
    ++failureCount();
    std::fprintf(stderr, "    %s:%d: %s\n", file, line, message.c_str());
}

void checkTrue(bool value, const char* expression, const char* file, int line) {
    if (!value) {
        reportFailure(file, line, std::string("expected true: ") + expression);
    }
}

void checkNear(double actual, double expected, double epsilon, const char* actualExpr, const char* expectedExpr,
               const char* file, int line) {
    if (std::fabs(actual - expected) > epsilon) {
        char buffer[256];
        std::snprintf(buffer, sizeof(buffer), "%s ~= %s (%f != %f)", actualExpr, expectedExpr, actual, expected);
        reportFailure(file, line, buffer);
    }
}

int runAll(const char* suiteFilter) {
    int selected = 0;
    int failed = 0;

    for (const TestCase& test : registry()) {
        if (suiteFilter != nullptr && std::string(test.suite) != suiteFilter) {
            continue;
        }
        ++selected;
        const int before = failureCount();
        test.run();
        const bool ok = failureCount() == before;
        if (!ok) {
            ++failed;
        }
        std::printf("  %s %s.%s\n", ok ? "ok  " : "FAIL", test.suite, test.name);
    }

    if (selected == 0) {
        std::fprintf(stderr, "no tests matched suite '%s'\n", suiteFilter);
        return 1;
    }

    std::printf("%d/%d passed\n", selected - failed, selected);
    return failed == 0 ? 0 : 1;
}

}  // namespace pactest

int main(int argc, char** argv) {
    const char* suite = argc > 1 ? argv[1] : nullptr;
    return pactest::runAll(suite);
}