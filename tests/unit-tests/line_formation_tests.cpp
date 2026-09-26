// Unit tests for QoL line-draw formation (geometry + squad dispatch).

#include "test_harness.h"

#include "units/Extensions.h"
#include "units/Formation.h"
#include "world/TileMap.h"
#include "units/Unit.h"

namespace
{

Entity SpawnFoot(Registry &registry, int tileX, int tileY)
{
    Entity entity = registry.Create();
    Unit unit;
    unit.type = UnitType::RifleInfantry;
    unit.position = cc::ToRaylib(cc::TileToWorld(tileX, tileY));
    registry.Add(entity, unit);
    return entity;
}

} // namespace

void RunLineFormationTests()
{
    // --- geometry: endpoints inclusive, midpoint for one, empty for none ---
    CC_CHECK(formation::LineFormationPositions(0, { 0.0f, 0.0f }, { 100.0f, 0.0f }).empty());
    {
        const auto single = formation::LineFormationPositions(1, { 0.0f, 0.0f }, { 100.0f, 0.0f });
        CC_CHECK(single.size() == 1);
        CC_CHECK(CcNear(single[0].x, 50.0f) && CcNear(single[0].y, 0.0f));
    }
    {
        const auto ends = formation::LineFormationPositions(2, { 0.0f, 0.0f }, { 100.0f, 0.0f });
        CC_CHECK(ends.size() == 2);
        CC_CHECK(CcNear(ends[0].x, 0.0f) && CcNear(ends[1].x, 100.0f));
    }
    {
        const auto five = formation::LineFormationPositions(5, { 0.0f, 10.0f }, { 100.0f, 10.0f });
        CC_CHECK(five.size() == 5);
        for (std::size_t i = 0; i < five.size(); ++i)
        {
            CC_CHECK(CcNear(five[i].x, static_cast<float>(i) * 25.0f));
            CC_CHECK(CcNear(five[i].y, 10.0f));
        }
    }

    // --- dispatch: squad fans out along the drawn line ---
    {
        Registry registry;
        TileMap map(20, 15);
        OccupancyGrid occ(20, 15);
        const Entity a = SpawnFoot(registry, 10, 10);
        const Entity b = SpawnFoot(registry, 11, 10);
        const Entity c = SpawnFoot(registry, 10, 11);
        const std::vector<Entity> squad = { a, b, c };

        formation::IssueLineFormationMoveFP(registry, squad, map, occ,
                                 cc::ToRaylib(cc::TileToWorld(2, 2)),
                                 cc::ToRaylib(cc::TileToWorld(8, 2)));
        const Vector2 ta = FindMover(registry, a)->moveTarget;
        const Vector2 tb = FindMover(registry, b)->moveTarget;
        const Vector2 tc = FindMover(registry, c)->moveTarget;
        // Distinct tiles spread along row 2 (y = 128), x increasing.
        CC_CHECK(ta.x != tb.x || ta.y != tb.y);
        CC_CHECK(tb.x != tc.x || tb.y != tc.y);
        CC_CHECK(ta.x != tc.x || ta.y != tc.y);
        CC_CHECK(ta.y == 128.0f && tb.y == 128.0f && tc.y == 128.0f);
        CC_CHECK(ta.x < tb.x && tb.x < tc.x);
        CC_CHECK(FindMover(registry, a)->hasPath);
    }

    // --- missing IDs don't shift surviving slots ---
    {
        Registry registry;
        TileMap map(20, 15);
        OccupancyGrid occ(20, 15);
        const Entity a = SpawnFoot(registry, 10, 10);
        const std::vector<Entity> squad = { a, kInvalidEntity };
        formation::IssueLineFormationMoveFP(registry, squad, map, occ,
                                 cc::ToRaylib(cc::TileToWorld(2, 2)),
                                 cc::ToRaylib(cc::TileToWorld(8, 2)));
        CC_CHECK(FindMover(registry, a)->hasPath); // no crash, slot kept
    }
}
