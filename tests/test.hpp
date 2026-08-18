#pragma once

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace test {

struct Registry {
    struct Case {
        std::string name;
        std::function<void()> fn;
    };
    static Registry& instance() {
        static Registry r;
        return r;
    }
    std::vector<Case> cases;
};

struct Register {
    Register(const char* name, std::function<void()> fn) {
        Registry::instance().cases.push_back({name, std::move(fn)});
    }
};

#define TEST(name)                                                             \
    static void test_##name();                                                 \
    static ::test::Register reg_##name(#name, test_##name);                    \
    static void test_##name()

// Aborts the current test with a formatted message.
#define FAIL(...)                                                              \
    do {                                                                       \
        std::printf("  FAIL %s:%d: ", __FILE__, __LINE__);                     \
        std::printf(__VA_ARGS__);                                              \
        std::printf("\n");                                                     \
        return;                                                                \
    } while (false)

#define CHECK(expr)                                                            \
    do {                                                                       \
        if (!(expr)) FAIL("CHECK failed: %s", #expr);                          \
    } while (false)

#define CHECK_EQUAL(a, b)                                                         \
    do {                                                                       \
        const auto va = (a);                                                   \
        const auto vb = (b);                                                   \
        if (!(va == vb))                                                       \
            FAIL("CHECK_EQUAL(%s, %s)", #a, #b);                                  \
    } while (false)

inline int run_all() {
    int failures = 0;
    for (const auto& tc : Registry::instance().cases) {
        std::printf("[ RUN  ] %s\n", tc.name.c_str());
        tc.fn();
        std::printf("[ DONE ] %s\n", tc.name.c_str());
    }
    return failures;
}

} // namespace test
