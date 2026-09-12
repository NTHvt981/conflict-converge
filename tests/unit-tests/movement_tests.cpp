// Unit tests for M2 Goal 4 move orders (snap, step, arrival, blocking).

#include "test_harness.h"

#include "TileMap.h"
#include "Unit.h"

void RunMovementTests()
{
    TileMap map(8, 8);

    // --- order snaps the destination to the tile corner ---
    Unit unit;
    unit.position = { 0.0f, 0.0f };
    IssueMoveOrder(unit, { 200.0f, 100.0f });
    CC_CHECK(unit.hasMoveOrder);
    CC_CHECK(unit.moveTarget.x == 192.0f);
    CC_CHECK(unit.moveTarget.y == 64.0f);

    // --- stepping advances toward the target, state + velocity set ---
    UpdateUnitMovement(unit, map, 100.0f, 1.0f);
    CC_CHECK(unit.hasMoveOrder);
    CC_CHECK(unit.state == UnitState::Moving);
    CC_CHECK(unit.position.x > 0.0f);
    CC_CHECK(unit.position.y > 0.0f);
    CC_CHECK(unit.velocity.x > 0.0f);

    // --- no order: no-op ---
    Unit idle;
    idle.position = { 64.0f, 64.0f };
    UpdateUnitMovement(idle, map, 100.0f, 1.0f);
    CC_CHECK(idle.position.x == 64.0f);
    CC_CHECK(idle.position.y == 64.0f);
    CC_CHECK(idle.state == UnitState::Idle);

    // --- arrival lands exactly snapped, order cleared ---
    Unit arriving;
    arriving.position = { 0.0f, 0.0f };
    IssueMoveOrder(arriving, { 64.0f, 0.0f });
    UpdateUnitMovement(arriving, map, 1000.0f, 1.0f); // overshoot step
    CC_CHECK(!arriving.hasMoveOrder);
    CC_CHECK(arriving.state == UnitState::Idle);
    CC_CHECK(arriving.position.x == 64.0f);
    CC_CHECK(arriving.position.y == 0.0f);

    // --- blocked tile stops the unit, order cancelled ---
    TileMap blocked(8, 8);
    blocked.Set({ 1, 0 }, TerrainType::Water);
    Unit walker;
    walker.position = { 0.0f, 0.0f };
    IssueMoveOrder(walker, { 192.0f, 0.0f }); // path crosses the water tile
    UpdateUnitMovement(walker, blocked, 64.0f, 1.0f);
    CC_CHECK(!walker.hasMoveOrder);
    CC_CHECK(walker.state == UnitState::Idle);
    CC_CHECK(walker.position.x == 0.0f); // never entered tile (1,0)
    CC_CHECK(walker.position.y == 0.0f);
}
