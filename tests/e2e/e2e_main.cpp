// Tier-1 end-to-end harness: drives the REAL Game loop (Init/Update/
// Shutdown) in a hidden window. Unit + integration suites cover sim logic;
// this covers the wiring they never touch: menu->match transitions, match
// teardown/restart, outcome flips, and the 2v2 commander path through the
// Game-owned objects. State assertions only, no screenshots (tier 2).
//
// Own binary (conflict-converge-e2e), excluded from the default test run:
// it needs a GPU window (hidden) and real time, so it is a local pre-commit
// gate, not CI. Run from the repo root so data/ resolves.

#include "../unit-tests/test_harness.h"

#include "E2EGame.h"
#include "Building.h" // RazeTeamBuildings arranges demolition-first outcomes
#include "MapFile.h"
#include "MathUtils.h" // TileToWorld for the mop-up march
#include "Menu.h"
#include "Pathfinder.h" // IssuePathOrder drives the mop-up force
#include "Registry.h"
#include "Unit.h"
#include "raylib.h"

#include <cstdio>
#include <string>
#include <vector>

namespace
{

int CountTeam(Registry &registry, int team)
{
    int count = 0;
    registry.Each<Unit>([&](Entity, const Unit &unit) {
        if (unit.health > 0.0f && unit.teamID == team)
        {
            ++count;
        }
    });
    return count;
}

// Arrange a wipe through the real death path: zero health and let the
// loop's sweep destroy the corpses (direct destroy would skip it).
void WipeTeam(Registry &registry, int team)
{
    registry.Each<Unit>([&](Entity, Unit &unit) {
        if (unit.teamID == team && unit.health > 0.0f)
        {
            unit.health = 0.0f;
        }
    });
}

// Raze a side's production first: otherwise the factory queue replaces the
// wiped units mid-scenario and the outcome never fires (correct game
// behavior — games end by demolition, not by one good battle). Real
// demolition (tiles freed) so the mop-up force can path into the base.
void RazeTeamBuildings(Registry &registry, TileMap &map, int team)
{
    std::vector<Entity> condemned;
    registry.Each<Building>([&](Entity id, const Building &building) {
        if (building.teamID == team)
        {
            condemned.push_back(id);
        }
    });
    for (Entity id : condemned)
    {
        DemolishBuilding(registry, map, id);
    }
}

void StepFrames(Game &game, int frames)
{
    for (int i = 0; i < frames; ++i)
    {
        game.Update();
    }
}

int FindMap(MenuFlow &menu, const std::string &name)
{
    for (int i = 0; i < static_cast<int>(menu.setup.maps.size()); ++i)
    {
        if (menu.setup.maps[static_cast<std::size_t>(i)].name == name)
        {
            return i;
        }
    }
    return -1;
}

// Boot -> menu -> setup -> start -> 600 live frames, no instant outcome.
void ScenarioStartAndSimulate()
{
    E2EGame game;
    game.Init();
    MenuFlow &menu = game.E2EMenu();
    CC_CHECK(menu.state == MenuState::MainMenu);
    menu.OpenSetup(ListMaps("data/maps"));
    CC_CHECK(menu.state == MenuState::SkirmishSetup);
    CC_CHECK(!menu.setup.maps.empty());
    menu.SelectMap(0);
    CC_CHECK(game.E2EStartSelectedMatch());
    CC_CHECK(game.IsWorldActive());
    CC_CHECK(menu.state == MenuState::Playing);
    StepFrames(game, 600);
    // Both sides fielded from frame one: still playing, world intact.
    CC_CHECK(menu.state == MenuState::Playing);
    CC_CHECK(game.IsWorldActive());
    CC_CHECK(CountTeam(game.E2ERegistry(), 0) > 0);
    CC_CHECK(CountTeam(game.E2ERegistry(), 1) > 0);
    game.Shutdown();
    std::printf("e2e: start-and-simulate ok\n");
}

// Wipe each side in turn through the real loop: Victory, then quit,
// restart, GameOver. Also exercises teardown/rebuild reference stability.
void ScenarioOutcomes()
{
    E2EGame game;
    game.Init();
    MenuFlow &menu = game.E2EMenu();
    menu.OpenSetup(ListMaps("data/maps"));
    menu.SelectMap(0);
    CC_CHECK(game.E2EStartSelectedMatch());
    RazeTeamBuildings(game.E2ERegistry(), game.E2EMap(), 1);
    WipeTeam(game.E2ERegistry(), 1);
    // Mop-up through the loop, not around it: the razed AI instantly
    // respawns harvesters while its ledger lasts, so a single external wipe
    // can never stick — kills must land inside the frame, between the AI
    // respawn and ShowOutcome, exactly like real play. The whole starting
    // army marches on the enemy home ONCE, then the camp holds itself:
    // idle units auto-acquire nearby spawns, so respawns die on the pad.
    // Two details earned by telemetry: campers hold position (a roaming
    // scout otherwise kites the whole camp via threat-priority acquisition
    // and nothing ever dies), and orders are issued once (re-issuing keeps
    // the camp marching instead of killing).
    const MapEntry *sel = menu.setup.SelectedMap();
    CC_CHECK(sel != nullptr);
    const cc::IVec2 enemyHome =
        sel != nullptr ? SpotsForMap(sel->path).aiHome : cc::IVec2{ 16, 9 };
    {
        Registry &registry = game.E2ERegistry();
        registry.Each<Unit>([&](Entity, Unit &unit) {
            if (unit.teamID == 0 && unit.type != UnitType::Engineer && unit.health > 0.0f)
            {
                SetStance(unit, Stance::Hold);
                IssueAttackMoveOrder(unit, game.E2EMap(),
                                     cc::ToRaylib(cc::TileToWorld(enemyHome.x, enemyHome.y)));
            }
        });
    }
    for (int round = 0; round < 30 && menu.state == MenuState::Playing; ++round)
    {
        StepFrames(game, 300);
    }
    CC_CHECK(menu.state == MenuState::Victory);
    game.E2EQuitToMenu();
    CC_CHECK(!game.IsWorldActive());
    CC_CHECK(menu.state == MenuState::MainMenu);
    menu.OpenSetup(ListMaps("data/maps"));
    menu.SelectMap(0);
    CC_CHECK(game.E2EStartSelectedMatch());
    CC_CHECK(game.IsWorldActive());
    RazeTeamBuildings(game.E2ERegistry(), game.E2EMap(), 0);
    WipeTeam(game.E2ERegistry(), 0);
    StepFrames(game, 120);
    CC_CHECK(menu.state == MenuState::GameOver);
    game.Shutdown();
    std::printf("e2e: outcomes ok\n");
}

// Twin Falls 2v2 through the Game-owned commanders: the allied guard
// (team 0) and both enemy guards (team 1) fielded from frame one.
void ScenarioAlliedAI()
{
    E2EGame game;
    game.Init();
    MenuFlow &menu = game.E2EMenu();
    menu.OpenSetup(ListMaps("data/maps"));
    const int twinFalls = FindMap(menu, "Twin Falls 2v2");
    CC_CHECK(twinFalls >= 0);
    if (twinFalls >= 0)
    {
        menu.SelectMap(twinFalls);
        CC_CHECK(game.E2EStartSelectedMatch());
        StepFrames(game, 60);
        CC_CHECK(menu.state == MenuState::Playing);
        // 4 player units + allied guard on team 0; two guards on team 1.
        CC_CHECK(CountTeam(game.E2ERegistry(), 0) >= 5);
        CC_CHECK(CountTeam(game.E2ERegistry(), 1) >= 2);
    }
    game.Shutdown();
    std::printf("e2e: allied-ai ok\n");
}

} // namespace

int main()
{
    // Hidden before Game::Init adds its own flags: flags accumulate, and
    // InitWindow then runs headless-visible with a live GL context.
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    ScenarioStartAndSimulate();
    ScenarioOutcomes();
    ScenarioAlliedAI();

    const TestStats &stats = CcTestStats();
    std::printf("e2e checks: %d, failures: %d\n", stats.checks, stats.failures);
    return stats.failures == 0 ? 0 : 1;
}
