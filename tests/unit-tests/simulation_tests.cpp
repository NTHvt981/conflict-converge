// Unit tests for Simulation (headless match tick): empty-world steps run
// clean, edge polls start empty, and the recording/viewer accessors work.

#include "test_harness.h"

#include "units/AICommander.h"
#include "app/data/Art.h"
#include "app/data/Audio.h"
#include "core/Event.h"
#include "economy/Building.h"
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
#include "units/Extensions.h"
#include "units/Unit.h"
#include "units/UnitFactory.h"

#include <optional>
#include <vector>

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
        world.AddKeyed<ProductionQueue>("bootcamp", ProductionCategory::Infantry);
        world.AddKeyed<ProductionQueue>("workshop", ProductionCategory::Vehicle);
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
        sim.emplace(world, engine, menu, worldState, damageNumbers,
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
        CC_CHECK(!fx.sim->HasBootcamp());
        CC_CHECK(!fx.sim->HasWorkshop());
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
        CC_CHECK(!fx.sim->HasBootcamp());
        CC_CHECK(!fx.sim->HasWorkshop());
        CC_CHECK(fx.sim->ReplayCount() == 5); // polls only: recording untouched
    }
    // --- round-robin production: alternating exits, units walk to their rally ---
    {
        Fixture fx;
        TileMap &map = fx.world.Get<TileMap>();
        const Entity bootA = PlaceBuilding(*fx.registry, map, BuildingType::Bootcamp, 0, 2, 2);
        const Entity bootB =
            PlaceBuilding(*fx.registry, map, BuildingType::Bootcamp, 0, 12, 10);
        CC_CHECK(bootA != kInvalidEntity && bootB != kInvalidEntity);
        UpdateBuildingConstruction(*fx.registry, 20.0f);
        fx.registry->Get<Building>(bootA)->rallyTile = { 2, 6 };
        fx.registry->Get<Building>(bootB)->rallyTile = { 12, 14 };
        fx.resources->AddIron(1000);
        fx.resources->AddOil(500);
        ProductionQueue &queue = fx.world.GetKeyed<ProductionQueue>("bootcamp");
        CC_CHECK(queue.Enqueue(*fx.resources, UnitType::RifleInfantry));
        CC_CHECK(queue.Enqueue(*fx.resources, UnitType::RifleInfantry));

        constexpr float kDt = 1.0f / 60.0f;
        std::vector<Entity> spawned;
        std::vector<cc::IVec2> spawnTiles;
        std::vector<bool> hadOrder;
        for (int i = 0; i < 3600 && spawned.size() < 2; ++i)
        {
            fx.sim->Step(kDt);
            fx.registry->Each<Unit>([&](Entity id, const Unit &unit) {
                if (unit.teamID != 0)
                {
                    return;
                }
                for (Entity seen : spawned)
                {
                    if (seen == id)
                    {
                        return;
                    }
                }
                spawned.push_back(id);
                spawnTiles.push_back(cc::WorldToTile(cc::ToGlm(unit.position)));
                const Mover *mover = FindMover(*fx.registry, id);
                hadOrder.push_back(mover != nullptr &&
                                   (mover->hasMoveOrder || mover->hasPath));
            });
        }
        CC_CHECK(spawned.size() == 2);
        CC_CHECK(spawnTiles[0] == cc::IVec2(3, 4));
        CC_CHECK(spawnTiles[1] == cc::IVec2(13, 12));
        CC_CHECK(hadOrder[0] && hadOrder[1]);

        bool arrived = false;
        for (int i = 0; i < 3600 && !arrived; ++i)
        {
            fx.sim->Step(kDt);
            arrived = true;
            for (std::size_t k = 0; k < spawned.size(); ++k)
            {
                const Unit *unit = fx.registry->Get<Unit>(spawned[k]);
                const cc::IVec2 want = (k == 0) ? cc::IVec2(2, 6) : cc::IVec2(12, 14);
                if (unit == nullptr || !(cc::WorldToTile(cc::ToGlm(unit->position)) == want))
                {
                    arrived = false;
                }
            }
        }
        CC_CHECK(arrived);
    }
}
