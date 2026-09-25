// Unit tests for QoL shared retreat (RetreatIfLowHP): threshold,
// fighting-withdrawal shape, Engineer exclusion, player opt-in gating.

#include "test_harness.h"

#include "Building.h"
#include "Extensions.h"
#include "TileMap.h"
#include "Unit.h"
#include "UnitStats.h"

namespace
{

Entity SpawnTeam(Registry &registry, UnitType type, int team, int tileX, int tileY, float hpFrac)
{
    Entity entity = registry.Create();
    Unit unit;
    unit.type = type;
    ApplyBaseStats(unit);
    unit.teamID = team;
    unit.position = cc::ToRaylib(cc::TileToWorld(tileX, tileY));
    unit.health = BaseStats(type).health * hpFrac;
    registry.Add(entity, unit);
    registry.Add(entity, Orders{});
    registry.Add(entity, Mover{});
    return entity;
}

Vector2 HomeTile(int tileX, int tileY)
{
    return cc::ToRaylib(cc::TileToWorld(tileX, tileY));
}

Entity SpawnBase(Registry &registry, int tileX, int tileY)
{
    Entity entity = registry.Create();
    Building base;
    base.teamID = 0;
    base.type = BuildingType::Base;
    base.state = BuildingState::Operational;
    base.tileX = tileX;
    base.tileY = tileY;
    registry.Add(entity, base);
    return entity;
}

} // namespace

void RunRetreatTests()
{
    // --- below threshold + opted in: fighting withdrawal toward home ---
    {
        Registry registry;
        TileMap map(10, 10);
        const Entity id = SpawnTeam(registry, UnitType::RifleInfantry, 0, 8, 8, 0.2f);
        Unit &unit = *registry.Get<Unit>(id);
        Orders &orders = GetOrders(registry, id);
        orders.autoRetreat = true;

        RetreatIfLowHP(registry, map, nullptr, HomeTile(1, 1), 0, kRetreatHealthFraction,
                       true);
        CC_CHECK(orders.attackMove); // withdrawal fights: not a plain move
        CC_CHECK(FindMover(registry, id)->hasPath || FindMover(registry, id)->hasMoveOrder);
        CC_CHECK(cc::WorldToTile(cc::ToGlm(orders.attackMoveDest)) == cc::IVec2(1, 1));
    }

    // --- healthy units hold regardless of the flag ---
    {
        Registry registry;
        TileMap map(10, 10);
        const Entity id = SpawnTeam(registry, UnitType::RifleInfantry, 0, 8, 8, 1.0f);
        Unit &unit = *registry.Get<Unit>(id);
        Orders &orders = GetOrders(registry, id);
        orders.autoRetreat = true;

        RetreatIfLowHP(registry, map, nullptr, HomeTile(1, 1), 0, kRetreatHealthFraction,
                       true);
        CC_CHECK(!orders.attackMove);
        CC_CHECK(!FindMover(registry, id)->hasMoveOrder && !FindMover(registry, id)->hasPath);
    }

    // --- Engineers never retreat, even flagged and bleeding ---
    {
        Registry registry;
        TileMap map(10, 10);
        const Entity id = SpawnTeam(registry, UnitType::Engineer, 0, 8, 8, 0.1f);
        Unit &unit = *registry.Get<Unit>(id);
        Orders &orders = GetOrders(registry, id);
        orders.autoRetreat = true;

        RetreatIfLowHP(registry, map, nullptr, HomeTile(1, 1), 0, kRetreatHealthFraction,
                       true);
        CC_CHECK(!orders.attackMove);
        CC_CHECK(!FindMover(registry, id)->hasMoveOrder && !FindMover(registry, id)->hasPath);
    }

    // --- opt-in gating: AI path (false) ignores the flag either way ---
    {
        Registry registry;
        TileMap map(10, 10);
        const Entity id = SpawnTeam(registry, UnitType::LightTank, 1, 8, 8, 0.2f);
        Orders &orders = GetOrders(registry, id);

        RetreatIfLowHP(registry, map, nullptr, HomeTile(1, 1), 1, kRetreatHealthFraction,
                       true);
        CC_CHECK(!orders.attackMove); // flag off: player pass leaves it alone
        RetreatIfLowHP(registry, map, nullptr, HomeTile(1, 1), 1, kRetreatHealthFraction,
                       false);
        CC_CHECK(orders.attackMove); // AI pass: threshold alone suffices
        CC_CHECK(cc::WorldToTile(cc::ToGlm(orders.attackMoveDest)) == cc::IVec2(1, 1));
    }

    // --- wrong team never retreats ---
    {
        Registry registry;
        TileMap map(10, 10);
        const Entity id = SpawnTeam(registry, UnitType::RifleInfantry, 1, 8, 8, 0.1f);
        Orders &orders = GetOrders(registry, id);

        RetreatIfLowHP(registry, map, nullptr, HomeTile(1, 1), 0, kRetreatHealthFraction,
                       false);
        CC_CHECK(!orders.attackMove);
    }

    // --- Bugfix: no-rally fallback prefers the Base nearest the army ---
    {
        Registry registry;
        TileMap map(20, 15); // center ~(10, 7): Base B is nearer the center,
        SpawnBase(registry, 16, 12); // ...but Base A is nearer the unit.
        SpawnBase(registry, 2, 2);
        const Entity id = SpawnTeam(registry, UnitType::RifleInfantry, 0, 2, 5, 0.2f);
        Orders &orders = GetOrders(registry, id);
        orders.autoRetreat = true;

        // Single retreating unit: army centroid == its own position, so the
        // "nearest to centroid" and "nearest to the unit" answers agree and
        // both disagree with the old "nearest to map center" (Base B).
        const Vector2 home = ResolvePlayerRetreatHome(registry, { 0.0f, 0.0f });
        CC_CHECK(cc::WorldToTile(cc::ToGlm(home)) == cc::IVec2(2, 2));

        RetreatIfLowHP(registry, map, nullptr, home, 0, kRetreatHealthFraction, true);
        CC_CHECK(orders.attackMove);
        CC_CHECK(cc::WorldToTile(cc::ToGlm(orders.attackMoveDest)) == cc::IVec2(2, 2));
    }

    // --- Bugfix: a placed rally still beats every Base ---
    {
        Registry registry;
        TileMap map(20, 15);
        SpawnBase(registry, 2, 2);
        SpawnBase(registry, 16, 12);
        SpawnTeam(registry, UnitType::RifleInfantry, 0, 2, 5, 0.2f);

        const Vector2 home = ResolvePlayerRetreatHome(registry, HomeTile(10, 3));
        CC_CHECK(cc::WorldToTile(cc::ToGlm(home)) == cc::IVec2(10, 3));
    }
}
