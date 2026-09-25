// Test runner: calls every unit-tests TU entry point, reports failures.
// Multi-system scenarios driven headless live in integration-tests/.
// Usage: conflict-converge-test.exe [--filter=Sub [--filter=...]] [--list]
// No args runs everything; --filter runs suites whose short name contains
// the substring (e.g. --filter=Art runs Art; --filter=Selection runs
// Selection + SelectionVisual). Repeatable (union). --list prints names.

#include "unit-tests/test_harness.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

void RunMathUtilsTests();
void RunEventTests();
void RunRegistryTests();
void RunCcAssertTests();
void RunTileMapTests();
void RunUnitSnapTests();
void RunCameraTests();
void RunSelectionTests();
void RunCursorTests();
void RunControlGroupTests();
void RunOrderQueueTests();
void RunAreaRepairTests();
void RunAttackGroundTests();
void RunPingTests();
void RunSimulationTests();
void RunRetreatTests();
void RunMovementTests();
void RunShortcutTests();
void RunHotkeyTests();
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
void RunFeedbackTests();
void RunIntegrationTests();
void RunPerfTests();
void RunAICommanderTests();
void RunFogTests();
void RunFogCombatTests();
void RunFontMetricsTests();
void RunDataRootTests();
void RunMapFileTests();
void RunAudioTests();
void RunArtTests();
void RunOrdersTests();
void RunCommandUiTests();
void RunSkirmishTests();
void RunFootprintTests();
void RunSpriteDataTests();
void RunUnitConfigTests();
void RunMovementStallTests();
void RunUnitStackResolutionTests();
void RunPrototypeSandboxMovementTests();
void RunFormationStackMoveTests();
void RunFormationDeadlockFixTests();
void RunLogTests();
void RunCheatTests();
void RunSubsystemTests();
void RunUnitCommandsTests();
void RunAutoRepairTests();

namespace
{

struct Suite
{
    const char *name;
    void (*run)();
};

// Same order as the historical flat call list below (suites are
// independent, but keep the sequence stable for log diffing).
const Suite kSuites[] = {
    { "MathUtils", RunMathUtilsTests },
    { "Event", RunEventTests },
    { "Registry", RunRegistryTests },
    { "CcAssert", RunCcAssertTests },
    { "TileMap", RunTileMapTests },
    { "UnitSnap", RunUnitSnapTests },
    { "Camera", RunCameraTests },
    { "Selection", RunSelectionTests },
    { "Cursor", RunCursorTests },
    { "ControlGroup", RunControlGroupTests },
    { "OrderQueue", RunOrderQueueTests },
    { "AreaRepair", RunAreaRepairTests },
    { "AttackGround", RunAttackGroundTests },
    { "Ping", RunPingTests },
    { "Simulation", RunSimulationTests },
    { "Retreat", RunRetreatTests },
    { "Movement", RunMovementTests },
    { "Shortcut", RunShortcutTests },
    { "Hotkey", RunHotkeyTests },
    { "InputManager", RunInputManagerTests },
    { "UnitAttribute", RunUnitAttributeTests },
    { "UnitStats", RunUnitStatsTests },
    { "Pathfind", RunPathfindTests },
    { "Targeting", RunTargetingTests },
    { "StateMachine", RunStateMachineTests },
    { "Formation", RunFormationTests },
    { "LineFormation", RunLineFormationTests },
    { "Factory", RunFactoryTests },
    { "SaveGame", RunSaveGameTests },
    { "Income", RunIncomeTests },
    { "Building", RunBuildingTests },
    { "Nodes", RunNodesTests },
    { "Minimap", RunMinimapTests },
    { "Hud", RunHudTests },
    { "Menu", RunMenuTests },
    { "SelectionVisual", RunSelectionVisualTests },
    { "Production", RunProductionTests },
    { "Combat", RunCombatTests },
    { "Hitbox", RunHitboxTests },
    { "AttackPhase", RunAttackPhaseTests },
    { "Feedback", RunFeedbackTests },
    { "Integration", RunIntegrationTests },
    { "Perf", RunPerfTests },
    { "AICommander", RunAICommanderTests },
    { "Fog", RunFogTests },
    { "FogCombat", RunFogCombatTests },
    { "FontMetrics", RunFontMetricsTests },
    { "DataRoot", RunDataRootTests },
    { "MapFile", RunMapFileTests },
    { "Audio", RunAudioTests },
    { "Art", RunArtTests },
    { "Orders", RunOrdersTests },
    { "CommandUi", RunCommandUiTests },
    { "Skirmish", RunSkirmishTests },
    { "Footprint", RunFootprintTests },
    { "SpriteData", RunSpriteDataTests },
    { "UnitConfig", RunUnitConfigTests },
    { "MovementStall", RunMovementStallTests },
    { "UnitStackResolution", RunUnitStackResolutionTests },
    { "PrototypeSandboxMovement", RunPrototypeSandboxMovementTests },
    { "FormationStackMove", RunFormationStackMoveTests },
    { "FormationDeadlockFix", RunFormationDeadlockFixTests },
    { "Log", RunLogTests },
    { "Cheat", RunCheatTests },
    { "Subsystem", RunSubsystemTests },
    { "UnitCommands", RunUnitCommandsTests },
    { "AutoRepair", RunAutoRepairTests },
};

bool MatchesFilter(const char *name, const std::vector<std::string> &filters)
{
    if (filters.empty())
    {
        return true;
    }
    for (const std::string &filter : filters)
    {
        if (std::strstr(name, filter.c_str()) != nullptr)
        {
            return true;
        }
    }
    return false;
}

void PrintUsage(const char *argv0)
{
    std::printf("usage: %s [--filter=Sub [--filter=...]] [--list]\n", argv0);
    std::printf("  no args: run every suite; --filter: only suites whose name\n");
    std::printf("  contains the substring (repeatable, union); --list: names.\n");
}

} // namespace

int main(int argc, char **argv)
{
    std::vector<std::string> filters;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--list")
        {
            for (const Suite &suite : kSuites)
            {
                std::printf("%s\n", suite.name);
            }
            return 0;
        }
        if (arg == "--help" || arg == "-h")
        {
            PrintUsage(argv[0]);
            return 0;
        }
        if (arg.rfind("--filter=", 0) == 0)
        {
            filters.push_back(arg.substr(9));
        }
        else if (arg == "--filter" && i + 1 < argc)
        {
            filters.push_back(argv[++i]);
        }
        else
        {
            std::printf("unknown argument: %s\n", arg.c_str());
            PrintUsage(argv[0]);
            return 2;
        }
    }

    int ran = 0;
    for (const Suite &suite : kSuites)
    {
        if (MatchesFilter(suite.name, filters))
        {
            suite.run();
            ++ran;
        }
    }
    if (!filters.empty() && ran == 0)
    {
        std::printf("no suites match the given filters\n");
        return 2;
    }

    const TestStats &stats = CcTestStats();
    std::printf("checks: %d, failures: %d\n", stats.checks, stats.failures);
    return stats.failures == 0 ? 0 : 1;
}
