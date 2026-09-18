// Unit tests for Simulation (headless match tick): empty-world steps run
// clean, edge polls start empty, and the recording/viewer accessors work.

#include "test_harness.h"

#include "AICommander.h"
#include "Art.h"
#include "Audio.h"
#include "Event.h"
#include "FogOfWar.h"
#include "GameCamera.h"
#include "Menu.h"
#include "Minimap.h"
#include "Nodes.h"
#include "Pings.h"
#include "Production.h"
#include "Registry.h"
#include "ResourceSystem.h"
#include "SaveGame.h"
#include "Simulation.h"
#include "TileMap.h"
#include "UnitFactory.h"

#include <optional>

namespace
{

struct Fixture
{
    Registry registry;
    TileMap map{ 20, 15 };
    OccupancyGrid occ{ 20, 15 };
    FogOfWar fog;
    ResourceNodes nodes;
    ProductionQueue queue;
    ResourceSystem resources;
    EventDispatcher events;
    UnitFactory factory{ registry, resources, events };
    AICommander ai{ registry, map, nodes, events, 1, AIDifficulty::Medium, { 0, 0 }, { 0, 0 } };
    AICommander allyAI{ registry, map, nodes, events, 0, AIDifficulty::Medium, { 0, 0 }, { 0, 0 } };
    AICommander enemyAI2{ registry, map, nodes, events, 1, AIDifficulty::Medium, { 0, 0 }, { 0, 0 } };
    Art art; // headless: never Init'ed (Step only touches pure pools/tints)
    Audio audio; // headless: Play guards on ready_
    Pings pings;
    MenuFlow menu;
    Minimap minimap;
    GameCamera camera;
    WorldState world{ &registry, &resources, &map, &camera, &nodes, &fog, &occ };
    DamageNumbers damageNumbers;
    Vector2 rallyPos = {};
    int autoAddGroupBit = -1;
    bool sandboxMode = true; // skip AI + outcome: covered by soak/integration tests
    bool worldIs2v2 = false;
    bool playerAutoRepair = false;
    float autoRepairCap = 1.0f;
    float shakeTrauma = 0.0f;
    MenuState lastOutcomeState = MenuState::Playing;
    std::optional<Simulation> sim; // emplaced: Simulation is non-copyable

    Fixture()
    {
        fog.Resize(20, 15);
        // No GL context headless: keep the minimap texture refresh disarmed
        // (Step would BeginTextureMode into an un-Init'ed target).
        minimap.refreshInterval = 1.0e9f;
        sim.emplace(registry, map, occ, fog, nodes, queue, factory, resources, ai,
                    allyAI, enemyAI2, art, audio, pings, menu, minimap, world,
                    damageNumbers, events, rallyPos, autoAddGroupBit, sandboxMode,
                    worldIs2v2, playerAutoRepair, autoRepairCap, shakeTrauma,
                    lastOutcomeState);
    }
};

} // namespace

void RunSimulationTests()
{
    // --- empty-world steps run clean, nothing accrued ---
    {
        Fixture fx;
        fx.sim->Step(1.0f / 60.0f);
        fx.sim->Step(1.0f / 60.0f);
        fx.sim->Step(1.0f / 60.0f);
        CC_CHECK(!fx.sim->HasFactory());
        CC_CHECK(fx.sim->ReplayCount() == 0);
        CC_CHECK(fx.shakeTrauma == 0.0f);
        CC_CHECK(fx.resources.iron == 0); // no Base: no base trickle
        CC_CHECK(fx.registry.EntityCount() == 0);
    }
    // --- match reset arms recording; stop + viewer count behave ---
    {
        Fixture fx;
        fx.sim->ResetForMatch();
        fx.sim->StopRecording();
        fx.sim->Step(1.0f / 60.0f);
        CC_CHECK(fx.sim->ReplayCount() == 0);
        fx.sim->SetReplayCount(5);
        CC_CHECK(fx.sim->ReplayCount() == 5);
        fx.sim->ResetEdgePolls();
        CC_CHECK(!fx.sim->HasFactory());
        CC_CHECK(fx.sim->ReplayCount() == 5); // polls only: recording untouched
    }
}
