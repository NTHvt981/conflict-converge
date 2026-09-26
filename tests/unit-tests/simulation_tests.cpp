// Unit tests for Simulation (headless match tick): empty-world steps run
// clean, edge polls start empty, and the recording/viewer accessors work.

#include "test_harness.h"

#include "units/AICommander.h"
#include "app/data/Art.h"
#include "app/data/Audio.h"
#include "core/Event.h"
#include "world/FogOfWar.h"
#include "app/ui/GameCamera.h"
#include "app/match/Menu.h"
#include "app/ui/Minimap.h"
#include "economy/Nodes.h"
#include "app/ui/Pings.h"
#include "economy/Production.h"
#include "core/Registry.h"
#include "economy/ResourceSystem.h"
#include "app/save/SaveGame.h"
#include "core/Simulation.h"
#include "core/Subsystem.h"
#include "world/TileMap.h"
#include "units/UnitFactory.h"

#include <optional>

namespace
{

struct Fixture
{
    Subsystems world;
    Subsystems engine;
    MenuFlow menu;
    GameCamera camera;
    WorldState worldState{};
    DamageNumbers damageNumbers;
    Vector2 rallyPos = {};
    int autoAddGroupBit = -1;
    bool sandboxMode = true; // skip AI + outcome: covered by soak/integration tests
    bool worldIs2v2 = false;
    float shakeTrauma = 0.0f;
    MenuState lastOutcomeState = MenuState::Playing;
    std::optional<Simulation> sim; // emplaced: Simulation is non-copyable

    // Non-owning views into world_ (bound in the ctor after the Adds) for
    // the checks below; sim binds the same objects.
    Registry *registry = nullptr;
    ResourceSystem *resources = nullptr;

    Fixture()
    {
        engine.Add<Art>(); // headless: never Init'ed (Step only touches pure pools/tints)
        engine.Add<Audio>(); // headless: Play guards on ready_
        engine.Add<EventDispatcher>();
        world.Add<Registry>();
        world.Add<ResourceSystem>();
        world.Add<TileMap>(20, 15);
        world.Add<OccupancyGrid>(20, 15);
        world.Add<FogOfWar>();
        world.Add<ResourceNodes>();
        world.Add<ProductionQueue>();
        EventDispatcher &events = engine.Get<EventDispatcher>();
        world.Add<UnitFactory>(world.Get<Registry>(), world.Get<ResourceSystem>(), events);
        world.AddKeyed<AICommander>("ai", world.Get<Registry>(), world.Get<TileMap>(),
                                    world.Get<ResourceNodes>(), events, 1,
                                    AIDifficulty::Medium, cc::IVec2{ 0, 0 },
                                    cc::IVec2{ 0, 0 });
        world.AddKeyed<AICommander>("ally", world.Get<Registry>(), world.Get<TileMap>(),
                                    world.Get<ResourceNodes>(), events, 0,
                                    AIDifficulty::Medium, cc::IVec2{ 0, 0 },
                                    cc::IVec2{ 0, 0 });
        world.AddKeyed<AICommander>("enemy2", world.Get<Registry>(), world.Get<TileMap>(),
                                    world.Get<ResourceNodes>(), events, 1,
                                    AIDifficulty::Medium, cc::IVec2{ 0, 0 },
                                    cc::IVec2{ 0, 0 });
        world.Add<Pings>();
        world.Add<Minimap>();
        world.Get<FogOfWar>().Resize(20, 15);
        // No GL context headless: keep the minimap texture refresh disarmed
        // (Step would BeginTextureMode into an un-Init'ed target).
        world.Get<Minimap>().refreshInterval = 1.0e9f;
        registry = &world.Get<Registry>();
        resources = &world.Get<ResourceSystem>();
        worldState = WorldState{ &world.Get<Registry>(), &world.Get<ResourceSystem>(),
                                &world.Get<TileMap>(), &camera,
                                &world.Get<ResourceNodes>(), &world.Get<FogOfWar>(),
                                &world.Get<OccupancyGrid>() };
        sim.emplace(world, engine, menu, worldState, damageNumbers, rallyPos,
                    autoAddGroupBit, sandboxMode, worldIs2v2, shakeTrauma, lastOutcomeState);
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
        CC_CHECK(fx.resources->iron == 0); // no Base: no base trickle
        CC_CHECK(fx.registry->EntityCount() == 0);
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
