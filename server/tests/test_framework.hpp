#pragma once

// Minimal test framework: TEST(name) registers a test, CHECK(cond) records a failure.
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace test {

struct Case {
    std::string name;
    std::function<void()> body;
};

inline std::vector<Case> &registry()
{
    static std::vector<Case> cases;
    return cases;
}

inline int &failures()
{
    static int n = 0;
    return n;
}

struct Register {
    Register(const std::string &name, std::function<void()> body) { registry().push_back({name, std::move(body)}); }
};

} // namespace test

#define TEST_CONCAT2(a, b) a##b
#define TEST_CONCAT(a, b) TEST_CONCAT2(a, b)
#define TEST(name)                                                                  \
    static void name();                                                             \
    static test::Register TEST_CONCAT(reg_, name)(#name, name);                     \
    static void name()

#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            ++test::failures();                                                     \
            std::cerr << "  " << __FILE__ << ":" << __LINE__ << ": CHECK(" #cond ") failed\n"; \
        }                                                                           \
    } while (0)

#define CHECK_EQ(a, b)                                                              \
    do {                                                                            \
        auto va = (a);                                                              \
        auto vb = (b);                                                              \
        if (!(va == vb)) {                                                          \
            ++test::failures();                                                     \
            std::cerr << "  " << __FILE__ << ":" << __LINE__ << ": " #a " == " #b   \
                      << " failed (" << va << " vs " << vb << ")\n";               \
        }                                                                           \
    } while (0)
