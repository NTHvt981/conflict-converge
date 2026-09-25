// Unit tests for formation movement (offsets + group orders).

#include "test_harness.h"

#include "Extensions.h"
#include "Formation.h"
#include "TileMap.h"
#include "Unit.h"      // Unit components under test
#include "UnitStats.h" // ApplyBaseStats

#include <cmath>
#include <set>

void RunFormationTests()
{
    // --- offset shapes ---
    CC_CHECK(formation::FormationOffsets(0).empty());

    const std::vector<cc::IVec2> one = formation::FormationOffsets(1);
    CC_CHECK(one.size() == 1);
    CC_CHECK(one[0] == cc::IVec2(0, 0));

    const std::vector<cc::IVec2> four = formation::FormationOffsets(4);
    CC_CHECK(four.size() == 4);
    CC_CHECK(four[0] == cc::IVec2(0, 0));
    CC_CHECK(four[1] == cc::IVec2(1, 0));
    CC_CHECK(four[2] == cc::IVec2(0, 1));
    CC_CHECK(four[3] == cc::IVec2(1, 1));

    // 7 units -> 3 columns, all slots unique.
    const std::vector<cc::IVec2> seven = formation::FormationOffsets(7);
    CC_CHECK(seven.size() == 7);
    std::set<std::pair<int, int>> slots;
    for (const cc::IVec2 &slot : seven)
    {
        CC_CHECK(slot.x >= 0 && slot.x < 3);
        slots.insert({ slot.x, slot.y });
    }
    CC_CHECK(slots.size() == 7);

    // --- group order: distinct slots around the anchor tile ---
    Registry registry;
    TileMap map(20, 15);
    std::vector<Entity> squad;
    for (int i = 0; i < 3; ++i)
    {
        Unit unit;
        unit.teamID = 0;
        unit.position = cc::ToRaylib(cc::TileToWorld(i, 0));
        squad.push_back(registry.Create());
        registry.Add(squad.back(), unit);
    }

    formation::IssueFormationMove(registry, squad, map, cc::ToRaylib(cc::TileToWorld(5, 5)));

    std::set<std::pair<int, int>> targets;
    int ordered = 0;
    registry.Each<Unit>([&](Entity id, const Unit &unit) {
        (void)unit;
        const Mover *mover = FindMover(registry, id);
        const cc::IVec2 tile = cc::WorldToTile(cc::ToGlm(mover->moveTarget));
        // Slot must be a live order near the anchor (offsets stay within the group box).
        CC_CHECK(tile.x >= 5 && tile.x <= 6);
        CC_CHECK(tile.y >= 5 && tile.y <= 6);
        targets.insert({ tile.x, tile.y });
        if (mover->hasMoveOrder || mover->hasPath)
        {
            ++ordered;
        }
    });
    CC_CHECK(ordered == 3);
    CC_CHECK(targets.size() == 3); // no two units stacked on one tile

    // --- missing IDs are skipped without shifting surviving slots ---
    std::vector<Entity> withGhost = squad;
    withGhost.push_back(kInvalidEntity);
    formation::IssueFormationMove(registry, withGhost, map, cc::ToRaylib(cc::TileToWorld(10, 10)));
    int reordered = 0;
    registry.Each<Unit>([&](Entity id, const Unit &unit) {
        (void)unit;
        const Mover *mover = FindMover(registry, id);
        if (mover != nullptr && (mover->hasMoveOrder || mover->hasPath))
        {
            ++reordered;
        }
    });
    CC_CHECK(reordered == 3);

    // --- : FormationOffsetsFP ---
    {
        // cellSize=1 matches legacy FormationOffsets
        const std::vector<cc::IVec2> fp1 = formation::FormationOffsetsFP(4, 1);
        CC_CHECK(fp1.size() == 4);
        CC_CHECK(fp1[0] == cc::IVec2(0, 0));
        CC_CHECK(fp1[1] == cc::IVec2(1, 0));
        CC_CHECK(fp1[2] == cc::IVec2(0, 1));
        CC_CHECK(fp1[3] == cc::IVec2(1, 1));
    }
    {
        // cellSize=2: 4 units get 2-tile spacing
        const std::vector<cc::IVec2> fp2 = formation::FormationOffsetsFP(4, 2);
        CC_CHECK(fp2.size() == 4);
        CC_CHECK(fp2[0] == cc::IVec2(0, 0));
        CC_CHECK(fp2[1] == cc::IVec2(2, 0));
        CC_CHECK(fp2[2] == cc::IVec2(0, 2));
        CC_CHECK(fp2[3] == cc::IVec2(2, 2));
    }
    {
        // cellSize=0 or negative returns empty
        CC_CHECK(formation::FormationOffsetsFP(4, 0).empty());
        CC_CHECK(formation::FormationOffsetsFP(4, -1).empty());
    }
    {
        // 6 units, cellSize=2: 3 columns × 2 rows, all unique
        const std::vector<cc::IVec2> fp6 = formation::FormationOffsetsFP(6, 2);
        CC_CHECK(fp6.size() == 6);
        std::set<std::pair<int, int>> seen;
        for (const auto &v : fp6)
        {
            seen.insert({ v.x, v.y });
        }
        CC_CHECK(seen.size() == 6);
        // All offsets are multiples of 2
        for (const auto &v : fp6)
        {
            CC_CHECK(v.x % 2 == 0);
            CC_CHECK(v.y % 2 == 0);
        }
    }

    // --- : IssueFormationMoveFP with 2x2 vehicles ---
    {
        Registry fpRegistry;
        TileMap fpMap(20, 15);
        OccupancyGrid fpOcc(20, 15);
        std::vector<Entity> vehicles;
        for (int i = 0; i < 2; ++i)
        {
            Unit unit;
            unit.type = UnitType::HeavyTank;
            ApplyBaseStats(unit);
            unit.teamID = 0;
            unit.position = cc::ToRaylib(cc::TileToWorld(i * 3, 0));
            vehicles.push_back(fpRegistry.Create());
            fpRegistry.Add(vehicles.back(), unit);
        }
        formation::IssueFormationMoveFP(fpRegistry, vehicles, fpMap, fpOcc,
                                        cc::ToRaylib(cc::TileToWorld(10, 10)));
        // Both units should have orders
        int orderedCount = 0;
        fpRegistry.Each<Unit>([&](Entity id, const Unit &unit) {
            (void)unit;
            const Mover *mover = FindMover(fpRegistry, id);
            if (mover != nullptr && (mover->hasMoveOrder || mover->hasPath))
            {
                ++orderedCount;
            }
        });
        CC_CHECK(orderedCount == 2);
    }

    // --- QoL slowest-speed: squad capped at the minimum, cleared after ---
    {
        Registry capRegistry;
        TileMap capMap(20, 15);
        OccupancyGrid capOcc(20, 15);
        std::vector<Entity> mixed;
        for (int i = 0; i < 3; ++i)
        {
            Unit unit;
            unit.type = UnitType::RifleInfantry;
            ApplyBaseStats(unit);
            unit.teamID = 0;
            unit.position = cc::ToRaylib(cc::TileToWorld(i * 3, 0));
            mixed.push_back(capRegistry.Create());
            capRegistry.Add(mixed.back(), unit);
        }
        capRegistry.Get<Unit>(mixed[0])->speed = 200.0f;
        capRegistry.Get<Unit>(mixed[1])->speed = 50.0f; // the minimum
        capRegistry.Get<Unit>(mixed[2])->speed = 100.0f;
        formation::IssueFormationMoveFP(capRegistry, mixed, capMap, capOcc,
                                        cc::ToRaylib(cc::TileToWorld(10, 10)), true);
        for (Entity id : mixed)
        {
            CC_CHECK(FindMover(capRegistry, id)->speedCapPixelsPerSec == 50.0f);
            CC_CHECK(EffectiveSpeed(*capRegistry.Get<Unit>(id), *FindMover(capRegistry, id)) ==
                     50.0f);
        }
        // End to end: the 200-speed unit advances exactly 50px in one tick.
        const Vector2 before = capRegistry.Get<Unit>(mixed[0])->position;
        UpdateUnit(mixed[0], capRegistry, capMap, 1.0f);
        const Vector2 after = capRegistry.Get<Unit>(mixed[0])->position;
        const float dx = after.x - before.x;
        const float dy = after.y - before.y;
        CC_CHECK(CcNear(std::sqrt(dx * dx + dy * dy), 50.0f));
        // Without the flag the cap is explicitly cleared, never stale.
        formation::IssueFormationMoveFP(capRegistry, mixed, capMap, capOcc,
                                        cc::ToRaylib(cc::TileToWorld(12, 12)), false);
        for (Entity id : mixed)
        {
            CC_CHECK(FindMover(capRegistry, id)->speedCapPixelsPerSec == -1.0f);
        }
    }

    // --- Bottleneck assignment: selection order is ignored ---
    // A(0,0) B(1,0) C(0,1) D(1,1) ordered to anchor (10,10) with the squad
    // list reversed (D first). The fair match is identity by position:
    // A->(10,10) B->(11,10) C->(10,11) D->(11,11) even though D was picked
    // first; index-order issue would give D->(10,10) with max cost 294
    // instead of 280.
    {
        Registry asRegistry;
        TileMap asMap(30, 30);
        OccupancyGrid asOcc(30, 30);
        const cc::IVec2 starts[4] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } };
        Entity byPos[4];
        for (int i = 0; i < 4; ++i)
        {
            Unit unit;
            unit.type = UnitType::RifleInfantry;
            ApplyBaseStats(unit);
            unit.teamID = 0;
            unit.position = cc::ToRaylib(cc::TileToWorld(starts[i].x, starts[i].y));
            byPos[i] = asRegistry.Create();
            asRegistry.Add(byPos[i], unit);
        }
        // Reversed pick order: D, C, B, A.
        const std::vector<Entity> reversed{ byPos[3], byPos[2], byPos[1], byPos[0] };
        formation::IssueFormationMoveFP(asRegistry, reversed, asMap, asOcc,
                                        cc::ToRaylib(cc::TileToWorld(10, 10)), false);
        const cc::IVec2 want[4] = { { 10, 10 }, { 11, 10 }, { 10, 11 }, { 11, 11 } };
        for (int i = 0; i < 4; ++i)
        {
            const Mover *u = FindMover(asRegistry, byPos[i]);
            CC_CHECK(u->hasMoveOrder || u->hasPath);
            CC_CHECK(cc::WorldToTile(cc::ToGlm(u->moveTarget)) == want[i]);
        }
    }
}
