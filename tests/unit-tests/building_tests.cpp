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

    // --- Phase 5: BuildingEntrances ---
    {
        // 20x15 map, base at (5,5) footprint 2x2 → tiles (5,5)(6,5)(5,6)(6,6)
        TileMap map2(20, 15);
        auto entrances = BuildingEntrances(map2, 5, 5, 2, 2);
        // All 12 one-ring tiles should be walkable on an empty grass map
        CC_CHECK(static_cast<int>(entrances.size()) == 12);
        // Verify none of the footprint tiles appear
        for (const cc::IVec2 &e : entrances)
        {
            bool insideFootprint = (e.x >= 5 && e.x <= 6 && e.y >= 5 && e.y <= 6);
            CC_CHECK(!insideFootprint);
        }
    }
    {
        // Building at (0,0): corner reduces ring size (out-of-bounds excluded)
        TileMap map2(20, 15);
        auto entrances = BuildingEntrances(map2, 0, 0, 2, 2);
        // Only 5 walkable tiles: (2,0)(0,2)(1,2)(2,1)(2,2)
        // (0,1),(1,1) are inside; (-1,*) and (*,-1) are OOB
        CC_CHECK(static_cast<int>(entrances.size()) == 5);
    }
    {
        // Water blocks entrance tiles
        TileMap map2(20, 15);
        map2.Set({ 7, 5 }, TerrainType::Water);
        map2.Set({ 4, 5 }, TerrainType::Water);
        auto entrances = BuildingEntrances(map2, 5, 5, 2, 2);
        // (7,5) and (4,5) blocked → 12 - 2 = 10
        CC_CHECK(static_cast<int>(entrances.size()) == 10);
    }

    // --- Phase 5: BuildingAttackPositions ---
    {
        TileMap map2(20, 15);
        auto positions = BuildingAttackPositions(map2, 5, 5, 2, 2, 8);
        // 12 available, max 8 → should return exactly 8
        CC_CHECK(static_cast<int>(positions.size()) == 8);
        // All returned positions are walkable
        for (const cc::IVec2 &p : positions)
        {
            CC_CHECK(map2.InBounds(p));
            CC_CHECK(!map2.IsBlocked(p));
        }
    }
    {
        // Fewer available than max → returns all available
        TileMap map2(20, 15);
        // Block most tiles around the building
        for (int dy = -1; dy <= 2; ++dy)
        {
            for (int dx = -1; dx <= 2; ++dx)
            {
                if (dx >= 0 && dx < 2 && dy >= 0 && dy < 2)
                    continue;
                int x = 5 + dx;
                int y = 5 + dy;
                if (map2.InBounds({ x, y }))
                    map2.Set({ x, y }, TerrainType::Water);
            }
        }
        auto positions = BuildingAttackPositions(map2, 5, 5, 2, 2, 12);
        CC_CHECK(static_cast<int>(positions.size()) == 0);
    }
}
