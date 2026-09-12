// Test runner: calls every unit-tests TU entry point, reports failures.
// Real integration coverage (game loop, combat, economy, save/load) arrives in M7.

#include "unit-tests/test_harness.h"

#include <cstdio>

void RunMathUtilsTests();
void RunEventTests();
void RunRegistryTests();
void RunCcAssertTests();
void RunTileMapTests();
void RunUnitSnapTests();
void RunCameraTests();
void RunSelectionTests();
void RunMovementTests();
void RunShortcutTests();

int main()
{
    RunMathUtilsTests();
    RunEventTests();
    RunRegistryTests();
    RunCcAssertTests();
    RunTileMapTests();
    RunUnitSnapTests();
    RunCameraTests();
    RunSelectionTests();
    RunMovementTests();
    RunShortcutTests();

    const TestStats &stats = CcTestStats();
    std::printf("checks: %d, failures: %d\n", stats.checks, stats.failures);
    return stats.failures == 0 ? 0 : 1;
}
