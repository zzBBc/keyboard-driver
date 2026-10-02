#pragma once
// A tiny test harness (no external dependencies). A test is written as
//
//     TEST(thingDoesX) { CHECK(a == b); }
//
// and registers itself; main.cpp runs them all. Exit code 0 = every check passed.
#include <iostream>
#include <vector>

namespace testing {

struct Case {
    const char* name;
    void (*run)();
};

inline std::vector<Case>& cases() {
    static std::vector<Case> all;
    return all;
}
inline int& checks() {
    static int n = 0;
    return n;
}
inline int& failures() {
    static int n = 0;
    return n;
}

struct Registrar {
    Registrar(const char* name, void (*run)()) { cases().push_back({name, run}); }
};

}  // namespace testing

#define TEST(name)                                                  \
    static void name();                                             \
    static ::testing::Registrar registrar_##name(#name, name);      \
    static void name()

#define CHECK(cond)                                                                  \
    do {                                                                             \
        ++::testing::checks();                                                       \
        if (!(cond)) {                                                               \
            ++::testing::failures();                                                 \
            std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK failed: " #cond "\n"; \
        }                                                                            \
    } while (0)
