#pragma once

#include <cstdio>

// Minimal test harness: no external framework (integration-tests/ holds the
// multi-system suites).
// Each unit-tests TU defines a Run*Tests entry point; tests/main.cpp calls
// them all and returns non-zero on any failure.

struct TestStats
{
    int checks = 0;
    int failures = 0;
};

inline TestStats &CcTestStats()
{
    static TestStats stats;
    return stats;
}

#define CC_CHECK(cond)                                                                                   \
    do                                                                                                   \
    {                                                                                                    \
        ++CcTestStats().checks;                                                                          \
        if (!(cond))                                                                                     \
        {                                                                                                \
            ++CcTestStats().failures;                                                                    \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                                  \
        }                                                                                                \
    } while (0)

inline bool CcNear(float a, float b, float eps = 1e-5f)
{
    float d = a >= b ? a - b : b - a;
    return d <= eps;
}
