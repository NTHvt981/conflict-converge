// Unit tests for M2 Goal 2 unit snapping (Unit <-> 64x64 tile grid).

#include "test_harness.h"

#include "Unit.h"

void RunUnitSnapTests()
{
    // --- arbitrary position snaps to its tile's top-left corner ---
    Unit unit;
    unit.position = { 70.0f, 130.0f };
    SnapUnitToTile(unit);
    CC_CHECK(unit.position.x == 64.0f);
    CC_CHECK(unit.position.y == 128.0f);

    // --- origin and exact corners are fixed points ---
    Unit origin;
    origin.position = { 0.0f, 0.0f };
    SnapUnitToTile(origin);
    CC_CHECK(origin.position.x == 0.0f);
    CC_CHECK(origin.position.y == 0.0f);

    Unit corner;
    corner.position = { 192.0f, 64.0f };
    SnapUnitToTile(corner);
    CC_CHECK(corner.position.x == 192.0f);
    CC_CHECK(corner.position.y == 64.0f);

    // --- snapping only touches position; stats survive ---
    Unit stats;
    stats.position = { 100.0f, 100.0f };
    stats.health = 55.0f;
    stats.type = UnitType::LightTank;
    stats.isSelected = true;
    SnapUnitToTile(stats);
    CC_CHECK(stats.position.x == 64.0f);
    CC_CHECK(stats.position.y == 64.0f);
    CC_CHECK(stats.health == 55.0f);
    CC_CHECK(stats.type == UnitType::LightTank);
    CC_CHECK(stats.isSelected);

    // --- snapped unit sits on the tile WorldToTile reports ---
    cc::IVec2 tile = cc::WorldToTile(cc::ToGlm(stats.position));
    CC_CHECK(tile == cc::IVec2(1, 1));
}
