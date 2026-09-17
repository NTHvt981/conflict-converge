// Unit tests for M5 Goal 2 buildings (placement validation, demolish, income).

#include "test_harness.h"

#include "Building.h"
#include "Nodes.h"
#include "Selection.h"
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
    CC_CHECK(stored->state == BuildingState::UnderConstruction); // sites ramp, not instant
    CC_CHECK(stored->health == 0.0f);
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
    UpdateBuildingConstruction(economy, 20.0f); // sites -> Operational (game ticks this)
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

    // --- QoL auto-repair: funds-costed heal, pauses when broke ---
    {
        Registry yard;
        TileMap yardMap(20, 15);
        const Entity hut =
            PlaceBuilding(yard, yardMap, BuildingType::Base, 0, 1, 1);
        CC_CHECK(hut != kInvalidEntity);
        UpdateBuildingConstruction(yard, 20.0f); // Operational before wounding
        Building *wounded = yard.Get<Building>(hut);
        wounded->health = 300.0f; // 100 missing of 400
        ResourceSystem funds;
        funds.AddIron(1000);
        funds.AddOil(500);
        UpdateBuildingAutoRepair(yard, funds, 1.0f, 0, 1.0f);
        CC_CHECK(wounded->health == 315.0f); // 15 HP @ 0.5 iron/HP...
        CC_CHECK(funds.iron == 1000 - 8);    // ...rounded up per quantum
        CC_CHECK(funds.oil == 500);          // iron-only cost
        // Second tick keeps healing while funded.
        UpdateBuildingAutoRepair(yard, funds, 1.0f, 0, 1.0f);
        CC_CHECK(wounded->health == 330.0f);

        // Broke: paused, never partially charged.
        funds.iron = 0;
        UpdateBuildingAutoRepair(yard, funds, 1.0f, 0, 1.0f);
        CC_CHECK(wounded->health == 330.0f);
        CC_CHECK(funds.iron == 0);
        UpdateBuildingAutoRepair(yard, funds, 1.0f, 0, 1.0f);
        CC_CHECK(wounded->health == 330.0f); // still paused, no free heal

        // Cap 0 pauses like a master switch; other teams untouched.
        funds.AddIron(1000);
        UpdateBuildingAutoRepair(yard, funds, 1.0f, 0, 0.0f);
        CC_CHECK(wounded->health == 330.0f);
        wounded->teamID = 1;
        UpdateBuildingAutoRepair(yard, funds, 1.0f, 0, 1.0f);
        CC_CHECK(wounded->health == 330.0f);
        UpdateBuildingAutoRepair(yard, funds, 1.0f, -1, 1.0f);
        CC_CHECK(wounded->health == 345.0f); // -1 = every team
    }

    // --- construction: sites ramp 0 -> max, then go Operational exactly ---
    {
        Registry site;
        TileMap siteMap(20, 15);
        const Entity baseId = PlaceBuilding(site, siteMap, BuildingType::Base, 0, 1, 1);
        const Entity depotId =
            PlaceBuilding(site, siteMap, BuildingType::ResourceDepot, 0, 5, 5);
        Building *baseB = site.Get<Building>(baseId);
        Building *depotB = site.Get<Building>(depotId);
        CC_CHECK(baseB->state == BuildingState::UnderConstruction);
        CC_CHECK(baseB->health == 0.0f && baseB->maxHealth == 400.0f);
        CC_CHECK(BuildingBuildTime(BuildingType::Base) == 3.0f);
        CC_CHECK(BuildingBuildTime(BuildingType::ResourceDepot) == 2.0f);
        CC_CHECK(BuildingBuildTime(BuildingType::Factory) == 4.0f);
        // Half the depot's build time: proportional ramp, still building.
        UpdateBuildingConstruction(site, 1.0f);
        CC_CHECK(depotB->state == BuildingState::UnderConstruction);
        CC_CHECK(depotB->health == 100.0f); // 200 * 1/2
        CC_CHECK(baseB->state == BuildingState::UnderConstruction);
        CC_CHECK(baseB->health > 0.0f && baseB->health < baseB->maxHealth); // 400 * 1/3
        // Sites earn no income while building.
        ResourceSystem siteFunds;
        UpdateBaseIncome(site, siteFunds, 10.0f, 0);
        CC_CHECK(siteFunds.iron == 0 && siteFunds.oil == 0);
        // Sites can't be auto-repaired (gates check == Operational).
        siteFunds.AddIron(1000);
        UpdateBuildingAutoRepair(site, siteFunds, 10.0f, 0, 1.0f);
        CC_CHECK(baseB->health > 0.0f && baseB->health < baseB->maxHealth &&
                 siteFunds.iron == 1000);
        // Depot finishes first (2s); base still ramping.
        UpdateBuildingConstruction(site, 1.0f);
        CC_CHECK(depotB->state == BuildingState::Operational);
        CC_CHECK(depotB->health == depotB->maxHealth); // exact, never overshoots
        CC_CHECK(baseB->state == BuildingState::UnderConstruction);
        // Base completes; income flows again.
        UpdateBuildingConstruction(site, 2.0f);
        CC_CHECK(baseB->state == BuildingState::Operational);
        CC_CHECK(baseB->health == baseB->maxHealth);
        ResourceSystem flowing;
        UpdateBaseIncome(site, flowing, 1.0f, 0);
        CC_CHECK(flowing.iron == 2 && flowing.oil == 1);
        // Non-positive dt never advances or completes.
        const Entity lateId =
            PlaceBuilding(site, siteMap, BuildingType::ResourceDepot, 0, 8, 8);
        UpdateBuildingConstruction(site, 0.0f);
        UpdateBuildingConstruction(site, -1.0f);
        CC_CHECK(site.Get<Building>(lateId)->state == BuildingState::UnderConstruction);
        CC_CHECK(site.Get<Building>(lateId)->health == 0.0f);
    }

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

    // --- QoL SelectAllBuildings: same-team same-type only, replaces ---
    {
        Registry lots;
        TileMap lotsMap(20, 15);
        const Entity facA =
            PlaceBuilding(lots, lotsMap, BuildingType::Factory, 0, 1, 1);
        const Entity facB =
            PlaceBuilding(lots, lotsMap, BuildingType::Factory, 0, 5, 5);
        const Entity depot =
            PlaceBuilding(lots, lotsMap, BuildingType::ResourceDepot, 0, 10, 10);
        const Entity foeFac =
            PlaceBuilding(lots, lotsMap, BuildingType::Factory, 1, 12, 12);
        CC_CHECK(facA != kInvalidEntity && facB != kInvalidEntity);
        CC_CHECK(depot != kInvalidEntity && foeFac != kInvalidEntity);
        CC_CHECK(SelectAllBuildings(lots, BuildingType::Factory, 0) == 2);
        CC_CHECK(lots.Get<Building>(facA)->isSelected);
        CC_CHECK(lots.Get<Building>(facB)->isSelected);
        CC_CHECK(!lots.Get<Building>(depot)->isSelected);
        CC_CHECK(!lots.Get<Building>(foeFac)->isSelected);
        CC_CHECK(SelectAllBuildings(lots, BuildingType::ResourceDepot, 0) == 1);
        CC_CHECK(!lots.Get<Building>(facA)->isSelected); // replaced
        CC_CHECK(lots.Get<Building>(depot)->isSelected);
    }

    // --- QoL CanPlaceBuilding: mirrors PlaceBuilding's validation ---
    {
        Registry yard;
        TileMap yardMap(20, 15);
        ResourceNodes yardNodes;
        CC_CHECK(CanPlaceBuilding(yardMap, &yardNodes, BuildingType::Base, 1, 1));
        CC_CHECK(!CanPlaceBuilding(yardMap, &yardNodes, BuildingType::Base, 19, 14));
        yardMap.Set({ 5, 5 }, TerrainType::Water);
        CC_CHECK(!CanPlaceBuilding(yardMap, &yardNodes, BuildingType::ResourceDepot, 5, 5));
        // Node tile: predicate refuses where the placer would strand iron.
        CC_CHECK(yardNodes.SpawnNode(yardMap, ResourceKind::Iron, { 8, 8 }, 200.0f, 10.0f));
        CC_CHECK(!CanPlaceBuilding(yardMap, &yardNodes, BuildingType::ResourceDepot, 8, 8));
        CC_CHECK(!CanPlaceBuilding(yardMap, &yardNodes, BuildingType::Base, 7, 7));
        // Null nodes = legacy grass-only check.
        CC_CHECK(CanPlaceBuilding(yardMap, nullptr, BuildingType::ResourceDepot, 8, 8));
        // ...but placing there still fails through the nodes-aware overload.
        CC_CHECK(PlaceBuilding(yard, yardMap, BuildingType::ResourceDepot, 0, 8, 8,
                               &yardNodes) == kInvalidEntity);
    }

    // --- QoL AreaBuildSlots: footprint-stepped tiling, inclusive range ---
    {
        // Depot (1x1) over a 3x2 range: every tile is a slot.
        const auto singles = AreaBuildSlots(BuildingType::ResourceDepot, { 1, 1 }, { 3, 2 });
        CC_CHECK(singles.size() == 6);
        CC_CHECK(singles.front() == cc::IVec2(1, 1) && singles.back() == cc::IVec2(3, 2));
        // Base (2x2) over 0..3: anchors at even tiles only, no overlaps.
        const auto quads = AreaBuildSlots(BuildingType::Base, { 0, 0 }, { 3, 3 });
        CC_CHECK(quads.size() == 4);
        CC_CHECK(quads[0] == cc::IVec2(0, 0) && quads[3] == cc::IVec2(2, 2));
        // Reversed range: empty, never negative-stepped.
        CC_CHECK(AreaBuildSlots(BuildingType::Base, { 3, 3 }, { 0, 0 }).empty());
    }

    // --- QoL BuildingFootprintRect: tile footprint as a world rect ---
    {
        Building depot;
        depot.type = BuildingType::ResourceDepot;
        depot.tileX = 3;
        depot.tileY = 4;
        const Rectangle depotRect = BuildingFootprintRect(depot);
        CC_CHECK(depotRect.x == 3.0f * 64.0f && depotRect.y == 4.0f * 64.0f);
        CC_CHECK(depotRect.width == 64.0f && depotRect.height == 64.0f);

        Building base;
        base.type = BuildingType::Base;
        base.tileX = 1;
        base.tileY = 1;
        const Rectangle baseRect = BuildingFootprintRect(base);
        CC_CHECK(baseRect.x == 64.0f && baseRect.y == 64.0f);
        CC_CHECK(baseRect.width == 128.0f && baseRect.height == 128.0f);
    }

    // --- QoL QueryBuildingsInRect: overlap + team filter ---
    {
        Registry query;
        TileMap queryMap(20, 15);
        const Entity inBase =
            PlaceBuilding(query, queryMap, BuildingType::Base, 0, 1, 1); // (64,64)-(192,192)
        const Entity inDepot = PlaceBuilding(query, queryMap, BuildingType::ResourceDepot,
                                             0, 5, 5); // single tile (5,5)
        const Entity foeBase =
            PlaceBuilding(query, queryMap, BuildingType::Base, 1, 10, 10); // far away
        CC_CHECK(inBase != kInvalidEntity && inDepot != kInvalidEntity &&
                 foeBase != kInvalidEntity);

        std::vector<Entity> found;
        // Rect overlapping only the team-0 base.
        QueryBuildingsInRect(query, { 0.0f, 0.0f, 200.0f, 200.0f }, 0, found);
        CC_CHECK(found.size() == 1 && found[0] == inBase);
        // Small rect strictly inside the depot tile.
        found.clear();
        QueryBuildingsInRect(query, { 5.0f * 64.0f, 5.0f * 64.0f, 10.0f, 10.0f }, 0,
                             found);
        CC_CHECK(found.size() == 1 && found[0] == inDepot);
        // Rect over nothing.
        found.clear();
        QueryBuildingsInRect(query, { 0.0f, 600.0f, 100.0f, 100.0f }, 0, found);
        CC_CHECK(found.empty());
        // Negative team: every team.
        found.clear();
        QueryBuildingsInRect(query, { 0.0f, 0.0f, 20.0f * 64.0f, 15.0f * 64.0f }, -1,
                             found);
        CC_CHECK(found.size() == 3);
        // Team 1 alone.
        found.clear();
        QueryBuildingsInRect(query, { 0.0f, 0.0f, 20.0f * 64.0f, 15.0f * 64.0f }, 1,
                             found);
        CC_CHECK(found.size() == 1 && found[0] == foeBase);
    }
}
