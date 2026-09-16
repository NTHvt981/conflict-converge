// Unit tests for QoL shared retreat (RetreatIfLowHP): threshold,
// fighting-withdrawal shape, Engineer exclusion, player opt-in gating.

#include "test_harness.h"

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
    return entity;
}

Vector2 HomeTile(int tileX, int tileY)
{
    return cc::ToRaylib(cc::TileToWorld(tileX, tileY));
}

} // namespace

void RunRetreatTests()
{
    // --- below threshold + opted in: fighting withdrawal toward home ---
    {
        Registry registry;
        TileMap map(10, 10);
        const Entity id = SpawnTeam(registry, UnitType::Infantry, 0, 8, 8, 0.2f);
        Unit &unit = *registry.Get<Unit>(id);
        unit.autoRetreat = true;

        RetreatIfLowHP(registry, map, nullptr, HomeTile(1, 1), 0, kRetreatHealthFraction,
                       true);
        CC_CHECK(unit.attackMove); // withdrawal fights: not a plain move
        CC_CHECK(unit.hasPath || unit.hasMoveOrder);
        CC_CHECK(cc::WorldToTile(cc::ToGlm(unit.attackMoveDest)) == cc::IVec2(1, 1));
    }

    // --- healthy units hold regardless of the flag ---
    {
        Registry registry;
        TileMap map(10, 10);
        const Entity id = SpawnTeam(registry, UnitType::Infantry, 0, 8, 8, 1.0f);
        Unit &unit = *registry.Get<Unit>(id);
        unit.autoRetreat = true;

        RetreatIfLowHP(registry, map, nullptr, HomeTile(1, 1), 0, kRetreatHealthFraction,
                       true);
        CC_CHECK(!unit.attackMove);
        CC_CHECK(!unit.hasMoveOrder && !unit.hasPath);
    }

    // --- Engineers never retreat, even flagged and bleeding ---
    {
        Registry registry;
        TileMap map(10, 10);
        const Entity id = SpawnTeam(registry, UnitType::Engineer, 0, 8, 8, 0.1f);
        Unit &unit = *registry.Get<Unit>(id);
        unit.autoRetreat = true;

        RetreatIfLowHP(registry, map, nullptr, HomeTile(1, 1), 0, kRetreatHealthFraction,
                       true);
        CC_CHECK(!unit.attackMove);
        CC_CHECK(!unit.hasMoveOrder && !unit.hasPath);
    }

    // --- opt-in gating: AI path (false) ignores the flag either way ---
    {
        Registry registry;
        TileMap map(10, 10);
        const Entity id = SpawnTeam(registry, UnitType::LightTank, 1, 8, 8, 0.2f);
        Unit &unit = *registry.Get<Unit>(id);

        RetreatIfLowHP(registry, map, nullptr, HomeTile(1, 1), 1, kRetreatHealthFraction,
                       true);
        CC_CHECK(!unit.attackMove); // flag off: player pass leaves it alone
        RetreatIfLowHP(registry, map, nullptr, HomeTile(1, 1), 1, kRetreatHealthFraction,
                       false);
        CC_CHECK(unit.attackMove); // AI pass: threshold alone suffices
        CC_CHECK(cc::WorldToTile(cc::ToGlm(unit.attackMoveDest)) == cc::IVec2(1, 1));
    }

    // --- wrong team never retreats ---
    {
        Registry registry;
        TileMap map(10, 10);
        const Entity id = SpawnTeam(registry, UnitType::Infantry, 1, 8, 8, 0.1f);
        Unit &unit = *registry.Get<Unit>(id);

        RetreatIfLowHP(registry, map, nullptr, HomeTile(1, 1), 0, kRetreatHealthFraction,
                       false);
        CC_CHECK(!unit.attackMove);
    }
}
