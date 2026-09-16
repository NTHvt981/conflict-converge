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
void RunControlGroupTests();
void RunOrderQueueTests();
void RunAttackGroundTests();
void RunPingTests();
void RunRetreatTests();
void RunMovementTests();
void RunShortcutTests();
void RunInputManagerTests();
void RunUnitAttributeTests();
void RunUnitStatsTests();
void RunPathfindTests();
void RunTargetingTests();
void RunStateMachineTests();
void RunFormationTests();
void RunLineFormationTests();
void RunFactoryTests();
void RunSaveGameTests();
void RunIncomeTests();
void RunBuildingTests();
void RunNodesTests();
void RunMinimapTests();
void RunHudTests();
void RunMenuTests();
void RunSelectionVisualTests();
void RunProductionTests();
void RunCombatTests();
void RunHitboxTests();
void RunAttackPhaseTests();
void RunCrushTests();
void RunFeedbackTests();
void RunIntegrationTests();
void RunPerfTests();
void RunAICommanderTests();
void RunFogTests();
void RunFogCombatTests();
void RunMapFileTests();
void RunAudioTests();
void RunArtTests();
void RunOrdersTests();
void RunCommandUiTests();
void RunSkirmishTests();
void RunFootprintTests();
void RunSpriteDataTests();
void RunMovementStallTests();

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
    RunControlGroupTests();
    RunOrderQueueTests();
    RunAttackGroundTests();
    RunPingTests();
    RunRetreatTests();
    RunMovementTests();
    RunShortcutTests();
    RunInputManagerTests();
    RunUnitAttributeTests();
    RunUnitStatsTests();
    RunPathfindTests();
    RunTargetingTests();
    RunStateMachineTests();
    RunFormationTests();
    RunLineFormationTests();
    RunFactoryTests();
    RunSaveGameTests();
    RunIncomeTests();
    RunBuildingTests();
    RunNodesTests();
    RunMinimapTests();
    RunHudTests();
    RunMenuTests();
    RunSelectionVisualTests();
    RunProductionTests();
    RunCombatTests();
    RunHitboxTests();
    RunAttackPhaseTests();
    RunCrushTests();
    RunFeedbackTests();
    RunIntegrationTests();
    RunPerfTests();
    RunAICommanderTests();
    RunFogTests();
    RunFogCombatTests();
    RunMapFileTests();
    RunAudioTests();
    RunArtTests();
    RunOrdersTests();
    RunCommandUiTests();
    RunSkirmishTests();
    RunFootprintTests();
    RunSpriteDataTests();
    RunMovementStallTests();

    const TestStats &stats = CcTestStats();
    std::printf("checks: %d, failures: %d\n", stats.checks, stats.failures);
    return stats.failures == 0 ? 0 : 1;
}
