// Unit tests for QoL attack-ground (approach, hold-at-range, continuous
// shelling, clean miss, order clearing).

#include "test_harness.h"

#include "Combat.h"
#include "Extensions.h"
#include "TileMap.h"
#include "Unit.h"
#include "UnitConfig.h"
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
            UpdateProjectiles(registry, map, 1.0f); // G2: shells fly after WindUp
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
        close.type = UnitType::RifleInfantry;
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
        ally.type = UnitType::RifleInfantry;
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

    // --- G2 arc: WindUp launches a shell, impact lands on the ordered tile ---
    {
        ResetActiveUnitConfigs(); // Artillery arcs by default (0.6s, splash 1)
        Registry registry;
        TileMap map(20, 15);
        const Entity id = SpawnArty(registry, 2, 2);
        Unit &gun = *registry.Get<Unit>(id);
        const Vector2 impact = cc::ToRaylib(cc::TileToWorld(6, 2));
        ResolveStrikeGround(registry, id, gun, impact);
        int shells = 0;
        Vector2 landing = {};
        registry.Each<Projectile>([&](Entity, const Projectile &shell) {
            ++shells;
            landing = shell.target;
        });
        CC_CHECK(shells == 1); // launch, not instant damage
        CC_CHECK(landing.x == impact.x && landing.y == impact.y);
        CC_CHECK(gun.cooldown == gun.cooldownTime); // cooldown stamped at launch
        ResetActiveUnitConfigs();
    }

    // --- G2 AoE: close enemy hit, distant enemy and friendlies spared ---
    {
        ResetActiveUnitConfigs();
        Registry registry;
        TileMap map(20, 15);
        const Entity id = SpawnArty(registry, 2, 2);
        Unit &gun = *registry.Get<Unit>(id);
        const Vector2 impact = cc::ToRaylib(cc::TileToWorld(6, 2));

        const Entity close = registry.Create();
        Unit near;
        near.type = UnitType::RifleInfantry;
        ApplyBaseStats(near);
        near.teamID = 1;
        near.position = impact;
        registry.Add(close, near);
        const float nearFull = near.health;

        const Entity far = registry.Create();
        Unit distant;
        distant.type = UnitType::RifleInfantry;
        ApplyBaseStats(distant);
        distant.teamID = 1;
        distant.position = cc::ToRaylib(cc::TileToWorld(9, 2)); // 3 tiles out
        registry.Add(far, distant);
        const float farFull = distant.health;

        const Entity pal = registry.Create();
        Unit ally;
        ally.type = UnitType::RifleInfantry;
        ApplyBaseStats(ally);
        ally.teamID = 0;
        ally.position = impact; // same-team at ground zero: no friendly fire
        registry.Add(pal, ally);
        const float palFull = ally.health;

        ResolveStrikeGround(registry, id, gun, impact);
        UpdateProjectiles(registry, map, 1.0f); // 0.9s flight resolves
        CC_CHECK(registry.Get<Unit>(close)->health < nearFull);
        CC_CHECK(registry.Get<Unit>(far)->health == farFull);
        CC_CHECK(registry.Get<Unit>(pal)->health == palFull);
        int left = 0;
        registry.Each<Projectile>([&](Entity, const Projectile &) { ++left; });
        CC_CHECK(left == 0); // landed shells are gone, never persisted
        ResetActiveUnitConfigs();
    }

    // --- G2 dodge: a fast vehicle escapes the snapshot landing point ---
    {
        ResetActiveUnitConfigs();
        Registry registry;
        TileMap map(20, 15);
        const Entity id = SpawnArty(registry, 2, 2);
        Unit &gun = *registry.Get<Unit>(id);
        const Entity foe = registry.Create();
        Unit runner;
        runner.type = UnitType::IFV; // 128px/s vs 0.6s flight, ±32px splash
        ApplyBaseStats(runner);
        runner.teamID = 1;
        runner.position = cc::ToRaylib(cc::TileToWorld(6, 2));
        registry.Add(foe, runner);
        const float fullHp = runner.health;

        ResolveStrikeGround(registry, id, gun,
                            { runner.position.x + 32.0f, runner.position.y + 32.0f });
        // Plain move orders never acquire, so the runner marches east flat-out.
        IssueMoveOrder(*registry.Get<Unit>(foe), cc::ToRaylib(cc::TileToWorld(9, 2)));
        for (int i = 0; i < 9; ++i)
        {
            UpdateUnit(foe, registry, map, 0.1f);
            UpdateProjectiles(registry, map, 0.1f);
        }
        CC_CHECK(registry.Get<Unit>(foe)->position.x >
                 cc::ToRaylib(cc::TileToWorld(6, 2)).x); // it moved
        CC_CHECK(registry.Get<Unit>(foe)->health == fullHp); // and dodged
        ResetActiveUnitConfigs();
    }

    // --- G2 tuned splash: foot cannot outwalk it ---
    {
        ResetActiveUnitConfigs();
        Registry registry;
        TileMap map(20, 15);
        const Entity id = SpawnArty(registry, 2, 2);
        Unit &gun = *registry.Get<Unit>(id);
        const Entity foe = registry.Create();
        Unit walker;
        walker.type = UnitType::Engineer; // 64px/s: 38px in 0.6s < ±32px + hitbox
        ApplyBaseStats(walker);
        walker.teamID = 1;
        walker.position = cc::ToRaylib(cc::TileToWorld(6, 2));
        registry.Add(foe, walker);
        const float fullHp = walker.health;
        ResolveStrikeGround(registry, id, gun,
                            { walker.position.x + 32.0f, walker.position.y + 32.0f });
        IssueMoveOrder(*registry.Get<Unit>(foe), cc::ToRaylib(cc::TileToWorld(9, 2)));
        for (int i = 0; i < 9; ++i)
        {
            UpdateUnit(foe, registry, map, 0.1f);
            UpdateProjectiles(registry, map, 0.1f);
        }
        CC_CHECK(registry.Get<Unit>(foe)->health < fullHp); // caught mid-splash
        ResetActiveUnitConfigs();
    }
}
