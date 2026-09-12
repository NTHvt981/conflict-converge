// Unit tests for M4 Goal 2 hitbox system (geometry, overlap, area query).

#include "test_harness.h"

#include "Combat.h"

void RunHitboxTests()
{
    // --- geometry: 32x32 body centered in the 64x64 tile ---
    Unit unit;
    unit.position = { 64.0f, 128.0f };
    const Rectangle box = HitboxOf(unit);
    CC_CHECK(box.x == 80.0f && box.y == 144.0f);
    CC_CHECK(box.width == 32.0f && box.height == 32.0f);

    // --- overlap: same tile touches; far tiles don't; edge contact counts ---
    Unit same = unit;
    CC_CHECK(HitboxesOverlap(unit, same));
    Unit neighbor = unit;
    neighbor.position = { 128.0f, 128.0f }; // adjacent tile: bodies 32px apart
    CC_CHECK(!HitboxesOverlap(unit, neighbor));
    Unit grazed = unit;
    grazed.position = { 64.0f + 31.0f, 128.0f }; // 1px body overlap
    CC_CHECK(HitboxesOverlap(unit, grazed));
    Unit edged = unit;
    edged.position = { 64.0f + 32.0f, 128.0f }; // edges exactly adjacent: no overlap
    CC_CHECK(!HitboxesOverlap(unit, edged));

    // --- area query: team filter, dead exclusion ---
    Registry registry;
    auto addAt = [&](int team, float x, float y, float hp) {
        Unit u;
        u.teamID = team;
        u.position = { x, y };
        u.health = hp;
        const Entity id = registry.Create();
        registry.Add(id, u);
        return id;
    };
    const Entity blue = addAt(0, 0.0f, 0.0f, 100.0f);
    addAt(1, 32.0f, 0.0f, 100.0f);
    addAt(0, 1000.0f, 1000.0f, 100.0f);
    addAt(0, 0.0f, 64.0f, 0.0f); // corpse: excluded

    std::vector<Entity> all;
    QueryUnitsInRect(registry, { 0.0f, 0.0f, 128.0f, 128.0f }, -1, all);
    CC_CHECK(all.size() == 2);

    std::vector<Entity> blues;
    QueryUnitsInRect(registry, { 0.0f, 0.0f, 128.0f, 128.0f }, 0, blues);
    CC_CHECK(blues.size() == 1);
    CC_CHECK(blues[0] == blue);

    std::vector<Entity> nowhere;
    QueryUnitsInRect(registry, { 5000.0f, 5000.0f, 64.0f, 64.0f }, -1, nowhere);
    CC_CHECK(nowhere.empty());
}
