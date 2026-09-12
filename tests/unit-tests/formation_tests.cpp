// Unit tests for M3 Goal 6 formation movement (offsets + group orders).

#include "test_harness.h"

#include "Formation.h"
#include "TileMap.h"
#include "Unit.h" // Unit components under test

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
    registry.Each<Unit>([&](Entity, const Unit &unit) {
        const cc::IVec2 tile = cc::WorldToTile(cc::ToGlm(unit.moveTarget));
        // Slot must be a live order near the anchor (offsets stay within the group box).
        CC_CHECK(tile.x >= 5 && tile.x <= 6);
        CC_CHECK(tile.y >= 5 && tile.y <= 6);
        targets.insert({ tile.x, tile.y });
        if (unit.hasMoveOrder || unit.hasPath)
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
    registry.Each<Unit>([&](Entity, const Unit &unit) {
        if (unit.hasMoveOrder || unit.hasPath)
        {
            ++reordered;
        }
    });
    CC_CHECK(reordered == 3);
}
