// Unit tests for QoL attack-ground (approach, hold-at-range, continuous
// shelling, clean miss, order clearing).

#include "test_harness.h"

#include "Combat.h"
#include "TileMap.h"
#include "Unit.h"
#include "UnitStats.h"

namespace
{

Entity SpawnArty(Registry &registry, int tileX, int tileY, int team = 0)
{
    Entity entity = registry.Create();
    Unit unit;
    unit.type = UnitType::Artillery;
    ApplyBaseStats(unit);
    unit.teamID = team;
    unit.position = cc::ToRaylib(cc::TileToWorld(tileX, tileY));
    registry.Add(entity, unit);
    return entity;
}

} // namespace

void RunAttackGroundTests()
{
    // --- out-of-range order marches, then holds and shells ---
    {
        Registry registry;
        TileMap map(20, 15);
        const Entity id = SpawnArty(registry, 2, 2);
        Unit &gun = *registry.Get<Unit>(id);

        IssueAttackGroundOrder(gun, map, cc::ToRaylib(cc::TileToWorld(15, 2)));
        CC_CHECK(gun.hasAttackGroundOrder);
        // Marches first: still moving after a few frames.
        for (int i = 0; i < 3; ++i)
        {
            UpdateUnit(id, registry, map, 1.0f);
        }
        CC_CHECK(gun.state == UnitState::Moving);
        // Arrives in range and starts the firing cycle.
        for (int i = 0; i < 60 && gun.state != UnitState::Attacking; ++i)
        {
            UpdateUnit(id, registry, map, 1.0f);
        }
        CC_CHECK(gun.state == UnitState::Attacking);
        CC_CHECK(gun.hasAttackGroundOrder); // standing order, not one-shot
    }

    // --- enemy at the impact point takes damage through the phase machine ---
    {
        Registry registry;
        TileMap map(20, 15);
        const Entity id = SpawnArty(registry, 2, 2);
        const Entity foe = registry.Create();
        Unit enemy;
        enemy.type = UnitType::LightTank;
        ApplyBaseStats(enemy);
        enemy.teamID = 1;
        enemy.position = cc::ToRaylib(cc::TileToWorld(15, 2));
        registry.Add(foe, enemy);
        const float fullHp = registry.Get<Unit>(foe)->health;

        IssueAttackGroundOrder(*registry.Get<Unit>(id), map,
                               cc::ToRaylib(cc::TileToWorld(15, 2)));
        for (int i = 0; i < 60 && registry.Get<Unit>(foe)->health >= fullHp; ++i)
        {
            UpdateUnit(id, registry, map, 1.0f);
        }
        CC_CHECK(registry.Get<Unit>(foe)->health < fullHp);
    }

    // --- empty ground: no crash, order persists (continuous, not one-shot) ---
    {
        Registry registry;
        TileMap map(20, 15);
        const Entity id = SpawnArty(registry, 2, 2);
        Unit &gun = *registry.Get<Unit>(id);

        IssueAttackGroundOrder(gun, map, cc::ToRaylib(cc::TileToWorld(10, 2)));
        for (int i = 0; i < 30; ++i)
        {
            UpdateUnit(id, registry, map, 1.0f); // must not crash, hits nothing
        }
        CC_CHECK(gun.hasAttackGroundOrder);
    }

    // --- ResolveGroundAttack: nearest in splash takes it, corpses excluded ---
    {
        Registry registry;
        TileMap map(10, 10);
        const Entity id = SpawnArty(registry, 1, 1);
        Unit &gun = *registry.Get<Unit>(id);
        const Entity near = registry.Create();
        Unit close;
        close.type = UnitType::Infantry;
        ApplyBaseStats(close);
        close.teamID = 1;
        close.position = cc::ToRaylib(cc::TileToWorld(5, 5));
        registry.Add(near, close);
        const float nearFull = close.health;

        ResolveGroundAttack(registry, gun, cc::ToRaylib(cc::TileToWorld(5, 5)));
        CC_CHECK(registry.Get<Unit>(near)->health < nearFull);

        // Ally at the impact point is never hit.
        const Entity pal = registry.Create();
        Unit ally;
        ally.type = UnitType::Infantry;
        ApplyBaseStats(ally);
        ally.teamID = 0;
        ally.position = cc::ToRaylib(cc::TileToWorld(5, 5));
        registry.Add(pal, ally);
        const float palFull = ally.health;
        ResolveGroundAttack(registry, gun, cc::ToRaylib(cc::TileToWorld(5, 5)));
        CC_CHECK(registry.Get<Unit>(pal)->health == palFull);
    }

    // --- new move orders clear shelling (exactly-one-active invariant) ---
    {
        Registry registry;
        TileMap map(10, 10);
        const Entity id = SpawnArty(registry, 1, 1);
        Unit &gun = *registry.Get<Unit>(id);

        IssueAttackGroundOrder(gun, map, cc::ToRaylib(cc::TileToWorld(8, 8)));
        CC_CHECK(gun.hasAttackGroundOrder);
        IssueMoveOrder(gun, cc::ToRaylib(cc::TileToWorld(1, 8)));
        CC_CHECK(!gun.hasAttackGroundOrder);
    }
}
