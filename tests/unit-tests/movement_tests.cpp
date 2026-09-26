// Unit tests for move orders (snap, step, arrival, blocking).

#include "test_harness.h"

#include "units/Extensions.h"
#include "world/TileMap.h"
#include "units/Unit.h"

void RunMovementTests()
{
    TileMap map(8, 8);

    // --- order snaps the destination to the tile corner ---
    Unit unit;
    Orders unitOrders;
    Mover unitMover;
    CombatState unitCombat;
    unit.position = { 0.0f, 0.0f };
    IssueMoveOrder(unit, unitOrders, unitMover, { 200.0f, 100.0f });
    CC_CHECK(unitMover.hasMoveOrder);
    CC_CHECK(unitMover.moveTarget.x == 192.0f);
    CC_CHECK(unitMover.moveTarget.y == 64.0f);

    // --- stepping advances toward the target, state + velocity set ---
    UpdateUnitMovement(unit, unitOrders, unitMover, unitCombat, map, 100.0f, 1.0f);
    CC_CHECK(unitMover.hasMoveOrder);
    CC_CHECK(unit.state == UnitState::Moving);
    CC_CHECK(unit.position.x > 0.0f);
    CC_CHECK(unit.position.y > 0.0f);
    CC_CHECK(unit.velocity.x > 0.0f);

    // --- no order: no-op ---
    Unit idle;
    Orders idleOrders;
    Mover idleMover;
    CombatState idleCombat;
    idle.position = { 64.0f, 64.0f };
    UpdateUnitMovement(idle, idleOrders, idleMover, idleCombat, map, 100.0f, 1.0f);
    CC_CHECK(idle.position.x == 64.0f);
    CC_CHECK(idle.position.y == 64.0f);
    CC_CHECK(idle.state == UnitState::Idle);

    // --- arrival lands exactly snapped, order cleared ---
    Unit arriving;
    Orders arrivingOrders;
    Mover arrivingMover;
    CombatState arrivingCombat;
    arriving.position = { 0.0f, 0.0f };
    IssueMoveOrder(arriving, arrivingOrders, arrivingMover, { 64.0f, 0.0f });
    UpdateUnitMovement(arriving, arrivingOrders, arrivingMover, arrivingCombat, map, 1000.0f,
                       1.0f); // overshoot step
    CC_CHECK(!arrivingMover.hasMoveOrder);
    CC_CHECK(arriving.state == UnitState::Idle);
    CC_CHECK(arriving.position.x == 64.0f);
    CC_CHECK(arriving.position.y == 0.0f);

    // --- blocked tile stops the unit, order cancelled ---
    TileMap blocked(8, 8);
    blocked.Set({ 1, 0 }, TerrainType::Water);
    Unit walker;
    Orders walkerOrders;
    Mover walkerMover;
    CombatState walkerCombat;
    walker.position = { 0.0f, 0.0f };
    IssueMoveOrder(walker, walkerOrders, walkerMover, { 192.0f, 0.0f }); // path crosses the water tile
    UpdateUnitMovement(walker, walkerOrders, walkerMover, walkerCombat, blocked, 64.0f, 1.0f);
    CC_CHECK(!walkerMover.hasMoveOrder);
    CC_CHECK(walker.state == UnitState::Idle);
    CC_CHECK(walker.position.x == 0.0f); // never entered tile (1,0)
    CC_CHECK(walker.position.y == 0.0f);

    // --- unit spawned inside a building footprint walks out, not stuck ---
    TileMap footprint(8, 8);
    footprint.Set({ 0, 0 }, TerrainType::Building);
    Unit trapped;
    Orders trappedOrders;
    Mover trappedMover;
    CombatState trappedCombat;
    trapped.position = { 0.0f, 0.0f };
    IssueMoveOrder(trapped, trappedOrders, trappedMover, { 320.0f, 0.0f });
    UpdateUnitMovement(trapped, trappedOrders, trappedMover, trappedCombat, footprint, 64.0f,
                       1.0f);
    CC_CHECK(trappedMover.hasMoveOrder); // order survives the escape step
    CC_CHECK(trapped.position.x > 0.0f); // stepped out of tile (0,0)
    // Full escape: keep walking until the order resolves off the footprint.
    for (int i = 0; i < 30 && (trappedMover.hasMoveOrder || trappedMover.hasPath); ++i)
    {
        UpdateUnitMovement(trapped, trappedOrders, trappedMover, trappedCombat, footprint, 64.0f,
                           1.0f);
    }
    CC_CHECK(!footprint.IsBlocked(cc::WorldToTile(cc::ToGlm(trapped.position))));

    // --- FacingFromVelocity: screen-space octants, y-down ---
    CC_CHECK(FacingFromVelocity({ 1.0f, 0.0f }) == Facing::Right);
    CC_CHECK(FacingFromVelocity({ 0.0f, 1.0f }) == Facing::Bottom);
    CC_CHECK(FacingFromVelocity({ -1.0f, 0.0f }) == Facing::Left);
    CC_CHECK(FacingFromVelocity({ 0.0f, -1.0f }) == Facing::Top);
    CC_CHECK(FacingFromVelocity({ 1.0f, 1.0f }) == Facing::BottomRight);
    CC_CHECK(FacingFromVelocity({ 1.0f, -1.0f }) == Facing::TopRight);
    CC_CHECK(FacingFromVelocity({ -1.0f, 1.0f }) == Facing::BottomLeft);
    CC_CHECK(FacingFromVelocity({ -1.0f, -1.0f }) == Facing::TopLeft);
    CC_CHECK(FacingFromVelocity({ 0.0f, 0.0f }) == Facing::Right); // degenerate
    CC_CHECK(FacingFromVelocity({ 10.0f, 4.0f }) == Facing::Right); // ~22deg
    CC_CHECK(FacingFromVelocity({ 10.0f, 5.0f }) == Facing::BottomRight); // ~27deg

    // --- facing tracks travel direction, persists on stop ---
    Unit marcher;
    Orders marcherOrders;
    Mover marcherMover;
    CombatState marcherCombat;
    marcher.position = { 0.0f, 0.0f };
    CC_CHECK(marcher.facing == Facing::Right); // default matches old rendering
    IssueMoveOrder(marcher, marcherOrders, marcherMover, { 0.0f, 192.0f }); // straight south
    UpdateUnitMovement(marcher, marcherOrders, marcherMover, marcherCombat, map, 64.0f, 1.0f);
    CC_CHECK(marcher.state == UnitState::Moving);
    CC_CHECK(marcher.facing == Facing::Bottom);
    UpdateUnitMovement(marcher, marcherOrders, marcherMover, marcherCombat, map, 10000.0f,
                       1.0f); // arrive
    CC_CHECK(marcher.state == UnitState::Idle);
    CC_CHECK(marcher.facing == Facing::Bottom); // kept on stop
}
