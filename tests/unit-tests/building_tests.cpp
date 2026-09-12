// Unit tests for M5 Goal 2 buildings (placement validation, demolish, income).

#include "test_harness.h"

#include "Building.h"
#include "TileMap.h"

#include <vector>

void RunBuildingTests()
{
    // --- footprints ---
    CC_CHECK(Footprint(BuildingType::Base) == cc::IVec2(2, 2));
    CC_CHECK(Footprint(BuildingType::ResourceDepot) == cc::IVec2(1, 1));
    CC_CHECK(Footprint(BuildingType::Factory) == cc::IVec2(2, 2));

    // --- valid placement marks terrain + stores the component ---
    Registry registry;
    TileMap map(20, 15);
    const Entity base = PlaceBuilding(registry, map, BuildingType::Base, 0, 1, 1);
    CC_CHECK(base != kInvalidEntity);
    const Building *stored = registry.Get<Building>(base);
    CC_CHECK(stored != nullptr);
    CC_CHECK(stored->type == BuildingType::Base);
    CC_CHECK(stored->state == BuildingState::Operational);
    CC_CHECK(stored->teamID == 0);
    CC_CHECK(map.Get({ 1, 1 }) == TerrainType::Building);
    CC_CHECK(map.Get({ 2, 2 }) == TerrainType::Building);
    CC_CHECK(map.Get({ 3, 1 }) == TerrainType::Grass);

    // --- overlap with the placed footprint is rejected ---
    CC_CHECK(PlaceBuilding(registry, map, BuildingType::Factory, 0, 2, 2) == kInvalidEntity);
    CC_CHECK(PlaceBuilding(registry, map, BuildingType::ResourceDepot, 0, 1, 1) ==
             kInvalidEntity);

    // --- water and out-of-bounds rejected ---
    map.Set({ 5, 5 }, TerrainType::Water);
    CC_CHECK(PlaceBuilding(registry, map, BuildingType::ResourceDepot, 0, 5, 5) ==
             kInvalidEntity);
    CC_CHECK(PlaceBuilding(registry, map, BuildingType::Base, 0, 19, 14) == kInvalidEntity);
    CC_CHECK(PlaceBuilding(registry, map, BuildingType::Base, 0, -1, 0) == kInvalidEntity);

    // --- demolish restores terrain and removes the entity ---
    CC_CHECK(DemolishBuilding(registry, map, base));
    CC_CHECK(!registry.IsAlive(base));
    CC_CHECK(map.Get({ 1, 1 }) == TerrainType::Grass);
    CC_CHECK(map.Get({ 2, 2 }) == TerrainType::Grass);
    CC_CHECK(!DemolishBuilding(registry, map, base)); // already gone: no-op
    CC_CHECK(!DemolishBuilding(registry, map, kInvalidEntity));

    // --- the freed footprint accepts a new building ---
    CC_CHECK(PlaceBuilding(registry, map, BuildingType::Factory, 1, 1, 1) != kInvalidEntity);

    // --- base income: per-Operational-Base trickle, team-filtered ---
    Registry economy;
    TileMap emap(20, 15);
    PlaceBuilding(economy, emap, BuildingType::Base, 0, 0, 0);
    PlaceBuilding(economy, emap, BuildingType::Factory, 0, 5, 5); // no income
    PlaceBuilding(economy, emap, BuildingType::Base, 1, 10, 10);
    ResourceSystem resources;
    UpdateBaseIncome(economy, resources, 1.0f);
    CC_CHECK(resources.iron == 4 && resources.oil == 2);

    ResourceSystem team0;
    UpdateBaseIncome(economy, team0, 1.0f, 0);
    CC_CHECK(team0.iron == 2 && team0.oil == 1);

    // --- demolished bases stop generating ---
    ResourceSystem after;
    std::vector<Entity> doomed;
    economy.Each<Building>([&](Entity id, const Building &) { doomed.push_back(id); });
    for (Entity id : doomed)
    {
        DemolishBuilding(economy, emap, id);
    }
    UpdateBaseIncome(economy, after, 10.0f);
    CC_CHECK(after.iron == 0 && after.oil == 0);
}
