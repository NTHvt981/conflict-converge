// Unit tests for M5 Goal 3 resource nodes (spawn, gather, deplete, respawn).

#include "test_harness.h"

#include "Nodes.h"
#include "TileMap.h"
#include "UnitStats.h" // ApplyBaseStats for live Engineers

namespace
{

Entity SpawnEngineer(Registry &registry, cc::IVec2 tile)
{
    Unit unit;
    unit.type = UnitType::Engineer;
    ApplyBaseStats(unit);
    unit.teamID = 0;
    unit.position = cc::ToRaylib(cc::TileToWorld(tile.x, tile.y));
    const Entity id = registry.Create();
    registry.Add(id, unit);
    return id;
}

} // namespace

void RunNodesTests()
{
    // --- spawn validation ---
    ResourceNodes nodes;
    TileMap map(20, 15);
    CC_CHECK(nodes.SpawnNode(map, ResourceKind::Iron, { 3, 3 }, 100.0f, 5.0f));
    CC_CHECK(nodes.Count() == 1);
    CC_CHECK(!nodes.SpawnNode(map, ResourceKind::Iron, { 3, 3 }, 100.0f, 5.0f)); // occupied
    map.Set({ 4, 4 }, TerrainType::Water);
    CC_CHECK(!nodes.SpawnNode(map, ResourceKind::Oil, { 4, 4 }, 100.0f, 5.0f)); // water
    CC_CHECK(!nodes.SpawnNode(map, ResourceKind::Oil, { 99, 99 }, 100.0f, 5.0f)); // off-map
    CC_CHECK(!nodes.SpawnNode(map, ResourceKind::Oil, { 6, 6 }, 0.0f, 5.0f)); // empty
    CC_CHECK(nodes.Count() == 1);
    CC_CHECK(nodes.FindAt({ 3, 3 }) != nullptr);
    CC_CHECK(nodes.FindAt({ 0, 0 }) == nullptr);

    // --- Engineers gather; others don't ---
    Registry registry;
    ResourceSystem resources;
    SpawnEngineer(registry, { 3, 3 });
    Unit scout;
    scout.type = UnitType::Infantry;
    ApplyBaseStats(scout);
    scout.teamID = 0;
    scout.position = cc::ToRaylib(cc::TileToWorld(3, 3));
    const Entity idle = registry.Create();
    registry.Add(idle, scout);

    nodes.GatherTick(registry, resources, 1.0f);
    CC_CHECK(resources.iron == 10); // one Engineer at 10/s; Infantry idles
    CC_CHECK(resources.oil == 0);
    CC_CHECK(nodes.FindAt({ 3, 3 })->amount == 90.0f);

    // --- depletion stops the flow; the node refills after its delay ---
    nodes.GatherTick(registry, resources, 9.0f); // 90 units over 9s
    CC_CHECK(resources.iron == 100);
    CC_CHECK(nodes.FindAt({ 3, 3 })->IsDepleted());
    nodes.GatherTick(registry, resources, 10.0f);
    CC_CHECK(resources.iron == 100); // dry node yields nothing

    nodes.Update(4.9f);
    CC_CHECK(nodes.FindAt({ 3, 3 })->IsDepleted());
    nodes.Update(0.1f);
    CC_CHECK(!nodes.FindAt({ 3, 3 })->IsDepleted());
    CC_CHECK(nodes.FindAt({ 3, 3 })->amount == 100.0f);

    // --- oil nodes feed the oil stockpile ---
    ResourceNodes oilField;
    CC_CHECK(oilField.SpawnNode(map, ResourceKind::Oil, { 7, 7 }, 40.0f, 5.0f));
    Registry crew;
    ResourceSystem oilBank;
    SpawnEngineer(crew, { 7, 7 });
    oilField.GatherTick(crew, oilBank, 1.0f);
    CC_CHECK(oilBank.oil == 8);
    CC_CHECK(oilBank.iron == 0);

    // --- fractional rates accumulate instead of truncating away ---
    ResourceNodes trickle;
    CC_CHECK(trickle.SpawnNode(map, ResourceKind::Iron, { 8, 8 }, 1000.0f, 5.0f));
    Registry solo;
    ResourceSystem pocket;
    SpawnEngineer(solo, { 8, 8 });
    for (int i = 0; i < 6; ++i) // 6 frames at 60fps
    {
        trickle.GatherTick(solo, pocket, 1.0f / 60.0f);
    }
    CC_CHECK(pocket.iron == 1); // 10/s * 0.1s, carried across ticks

    // --- dead Engineers gather nothing ---
    Registry morgue;
    const Entity corpse = SpawnEngineer(morgue, { 8, 8 });
    morgue.Get<Unit>(corpse)->health = 0.0f;
    ResourceSystem quiet;
    trickle.GatherTick(morgue, quiet, 5.0f);
    CC_CHECK(quiet.iron == 0);
}
