// Test runner: calls every unit-tests TU entry point, reports failures.
// Real integration coverage (game loop, combat, economy, save/load) arrives in M7.

#include "unit-tests/test_harness.h"

#include <cstdio>

void RunMathUtilsTests();
void RunEventTests();
void RunRegistryTests();
void RunCcAssertTests();
void RunTileMapTests();

int main()
{
    RunMathUtilsTests();
    RunEventTests();
    RunRegistryTests();
    RunCcAssertTests();
    RunTileMapTests();

    const TestStats &stats = CcTestStats();
    std::printf("checks: %d, failures: %d\n", stats.checks, stats.failures);
    return stats.failures == 0 ? 0 : 1;
}
