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

    // --- unit spawned inside a building footprint walks out, not stuck ---
    TileMap footprint(8, 8);
    footprint.Set({ 0, 0 }, TerrainType::Building);
    Unit trapped;
    trapped.position = { 0.0f, 0.0f };
    IssueMoveOrder(trapped, { 320.0f, 0.0f });
    UpdateUnitMovement(trapped, footprint, 64.0f, 1.0f);
    CC_CHECK(trapped.hasMoveOrder); // order survives the escape step
    CC_CHECK(trapped.position.x > 0.0f); // stepped out of tile (0,0)
    // Full escape: keep walking until the order resolves off the footprint.
    for (int i = 0; i < 30 && (trapped.hasMoveOrder || trapped.hasPath); ++i)
    {
        UpdateUnitMovement(trapped, footprint, 64.0f, 1.0f);
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
    marcher.position = { 0.0f, 0.0f };
    CC_CHECK(marcher.facing == Facing::Right); // default matches old rendering
    IssueMoveOrder(marcher, { 0.0f, 192.0f }); // straight south
    UpdateUnitMovement(marcher, map, 64.0f, 1.0f);
    CC_CHECK(marcher.state == UnitState::Moving);
    CC_CHECK(marcher.facing == Facing::Bottom);
    UpdateUnitMovement(marcher, map, 10000.0f, 1.0f); // arrive
    CC_CHECK(marcher.state == UnitState::Idle);
    CC_CHECK(marcher.facing == Facing::Bottom); // kept on stop

    // --- SeparateUnits: overlapping bodies fan out, corpses ignored ---
    Registry crowd;
    auto addBody = [&](float x, float y, float health) {
        const Entity e = crowd.Create();
        Unit body;
        body.position = { x, y };
        body.health = health;
        crowd.Add(e, body);
        return e;
    };
    const Entity left = addBody(0.0f, 0.0f, 100.0f);
    const Entity right = addBody(16.0f, 0.0f, 100.0f); // centers 16px apart
    const Entity far = addBody(500.0f, 500.0f, 100.0f);
    const Entity corpse = addBody(8.0f, 0.0f, 0.0f); // dead: never pushes
    SeparateUnits(crowd, 1.0f);
    // 16px gap needs 32: each side takes half (8px), far under the 96px cap.
    CC_CHECK(crowd.Get<Unit>(left)->position.x == -8.0f);
    CC_CHECK(crowd.Get<Unit>(right)->position.x == 24.0f);
    CC_CHECK(crowd.Get<Unit>(left)->position.y == 0.0f);
    CC_CHECK(crowd.Get<Unit>(far)->position.x == 500.0f);
    CC_CHECK(crowd.Get<Unit>(corpse)->position.x == 8.0f);

    // --- exact stacks split deterministically (order-agnostic checks) ---
    Registry stack;
    auto addStacked = [&](Registry &reg) {
        const Entity e = reg.Create();
        Unit body;
        body.position = { 100.0f, 100.0f };
        body.health = 100.0f;
        reg.Add(e, body);
        return e;
    };
    const Entity t0 = addStacked(stack);
    const Entity t1 = addStacked(stack);
    SeparateUnits(stack, 1.0f);
    const float x0 = stack.Get<Unit>(t0)->position.x;
    const float x1 = stack.Get<Unit>(t1)->position.x;
    CC_CHECK(x0 - x1 == 32.0f || x0 - x1 == -32.0f); // full 32px gap...
    CC_CHECK((x0 + x1) / 2.0f == 100.0f);           // ...centered, along x
    CC_CHECK(stack.Get<Unit>(t0)->position.y == 100.0f);
    CC_CHECK(stack.Get<Unit>(t1)->position.y == 100.0f);

    // --- push is speed-capped per frame ---
    Registry capped;
    auto addCapped = [&](float x) {
        const Entity e = capped.Create();
        Unit body;
        body.position = { x, 0.0f };
        body.health = 100.0f;
        capped.Add(e, body);
        return e;
    };
    const Entity p0 = addCapped(0.0f);
    const Entity p1 = addCapped(2.0f);
    SeparateUnits(capped, 0.01f); // cap = 0.96px per side
    const float gap =
        capped.Get<Unit>(p0)->position.x - capped.Get<Unit>(p1)->position.x;
    CC_CHECK(CcNear(gap, 2.0f + 2.0f * 0.96f) || CcNear(gap, -(2.0f + 2.0f * 0.96f)));

    // --- non-positive dt is a no-op ---
    const float before = capped.Get<Unit>(p0)->position.x;
    SeparateUnits(capped, 0.0f);
    SeparateUnits(capped, -1.0f);
    CC_CHECK(capped.Get<Unit>(p0)->position.x == before);
}
