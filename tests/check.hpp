// Shared harness for the core test suite (no framework dependency):
// a failure counter and the CHECK macro the test files share.
#pragma once

#include <cstdio>

extern int test_failures;

#define CHECK(cond, ...)                                       \
    do {                                                       \
        if (!(cond)) {                                         \
            ++test_failures;                                   \
            std::printf("FAIL %s:%d  ", __FILE__, __LINE__);   \
            std::printf(__VA_ARGS__);                          \
            std::printf("\n");                                \
        }                                                      \
    } while (0)
