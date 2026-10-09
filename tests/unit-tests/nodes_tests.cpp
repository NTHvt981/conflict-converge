// Unit tests for resource nodes (spawn, gather, deplete, respawn).

#include "test_harness.h"

#include "economy/Nodes.h"
#include "economy/Building.h"
#include "world/TileMap.h"
#include "units/Extensions.h"
#include "units/UnitStats.h" // ApplyBaseStats for live Engineers

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
    scout.type = UnitType::RifleInfantry;
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

    // --- negative respawn exhausts the node forever ---
    CC_CHECK(trickle.SpawnNode(map, ResourceKind::Iron, { 9, 9 }, 20.0f, -1.0f));
    Registry claim;
    SpawnEngineer(claim, { 9, 9 });
    trickle.GatherTick(claim, pocket, 3.0f); // 20 units at 10/s over 2s, then dry
    CC_CHECK(trickle.FindAt({ 9, 9 })->IsDepleted());
    trickle.Update(60.0f);
    CC_CHECK(trickle.FindAt({ 9, 9 })->IsDepleted());

    // --- dead Engineers gather nothing ---
    Registry morgue;
    const Entity corpse = SpawnEngineer(morgue, { 8, 8 });
    morgue.Get<Unit>(corpse)->health = 0.0f;
    ResourceSystem quiet;
    trickle.GatherTick(morgue, quiet, 5.0f);
    CC_CHECK(quiet.iron == 0);

    // --- loop harvesters fill at the field, ledger untouched until dock ---
    ResourceNodes loop;
    TileMap field(20, 15);
    CC_CHECK(loop.SpawnNode(field, ResourceKind::Iron, { 3, 3 }, 1000.0f, 5.0f));
    Registry crew2;
    ResourceSystem bank;
    CC_CHECK(PlaceBuilding(crew2, field, BuildingType::ResourceDepot, 0, 8, 8, &loop) !=
             kInvalidEntity);
    UpdateBuildingConstruction(crew2, 5.0f);
    const Entity hauler = SpawnEngineer(crew2, { 3, 3 });
    Harvester &plan = GetHarvester(crew2, hauler);
    plan.phase = HarvestPhase::Loading;
    plan.nodeTile = { 3, 3 };
    loop.GatherTick(crew2, bank, 5.0f);
    CC_CHECK(bank.iron == 0);
    CC_CHECK(plan.load == 80.0f);
    CC_CHECK(loop.FindAt({ 3, 3 })->amount == 920.0f);
    loop.GatherTick(crew2, bank, 5.0f);
    CC_CHECK(plan.load == 100.0f);
    CC_CHECK(plan.phase == HarvestPhase::ToDepot);

    // --- docked harvesters dump through the storage cap, overflow lost ---
    crew2.Get<Unit>(hauler)->position = cc::ToRaylib(cc::TileToWorld(7, 8));
    plan.phase = HarvestPhase::Unloading;
    loop.GatherTick(crew2, bank, 1.0f);
    CC_CHECK(bank.iron == 100);
    CC_CHECK(plan.load == 0.0f);
    CC_CHECK(plan.phase == HarvestPhase::ToField);
    bank.iron = 1490;
    plan.load = 100.0f;
    plan.phase = HarvestPhase::Unloading;
    loop.GatherTick(crew2, bank, 1.0f);
    CC_CHECK(bank.iron == 1500); // 500 base + 1000 depot; 90 lost
    CC_CHECK(plan.load == 90.0f);
    CC_CHECK(plan.phase == HarvestPhase::Unloading);

    // --- base income stalls at the cap ---
    CC_CHECK(PlaceBuilding(crew2, field, BuildingType::Base, 0, 12, 12, &loop) !=
             kInvalidEntity);
    UpdateBuildingConstruction(crew2, 5.0f);
    UpdateBaseIncome(crew2, bank, 60.0f, 0);
    CC_CHECK(bank.iron == 1500);

    // --- idle loop harvesters take orders to their field ---
    plan.phase = HarvestPhase::ToField;
    plan.load = 0.0f;
    plan.nodeTile = { 3, 3 };
    loop.UpdateHarvesters(crew2, field, nullptr, 1.0f / 60.0f, 0);
    const Mover *haulMover = FindMover(crew2, hauler);
    CC_CHECK(haulMover != nullptr && (haulMover->hasMoveOrder || haulMover->hasPath));

    // --- one harvester per field: the second takes the free node ---
    ResourceNodes shared;
    TileMap duplex(20, 15);
    CC_CHECK(shared.SpawnNode(duplex, ResourceKind::Iron, { 3, 3 }, 1000.0f, 5.0f));
    CC_CHECK(shared.SpawnNode(duplex, ResourceKind::Iron, { 6, 3 }, 1000.0f, 5.0f));
    Registry pair;
    ResourceSystem kitty;
    const Entity first = SpawnEngineer(pair, { 3, 3 });
    const Entity second = SpawnEngineer(pair, { 3, 3 });
    GetHarvester(pair, first).phase = HarvestPhase::ToField;
    GetHarvester(pair, first).nodeTile = { 3, 3 };
    GetHarvester(pair, second).phase = HarvestPhase::ToField;
    GetHarvester(pair, second).nodeTile = { 3, 3 };
    shared.UpdateHarvesters(pair, duplex, nullptr, 1.0f / 60.0f, 0);
    const cc::IVec2 firstTile = pair.Get<Harvester>(first)->nodeTile;
    const cc::IVec2 secondTile = pair.Get<Harvester>(second)->nodeTile;
    CC_CHECK(!(firstTile == secondTile));
    CC_CHECK(shared.FindAt(firstTile) != nullptr && shared.FindAt(secondTile) != nullptr);
    int marching = 0;
    for (Entity id : { first, second })
    {
        const Mover *mover = FindMover(pair, id);
        if (mover != nullptr && (mover->hasMoveOrder || mover->hasPath))
        {
            ++marching;
        }
    }
    CC_CHECK(marching == 1);
}
