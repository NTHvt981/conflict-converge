// Unit tests for M13 orders: attack-move, stances, patrol, repair (+ HP).

#include "test_harness.h"

#include "Building.h" // PlaceBuilding, BuildingMaxHealth
#include "TileMap.h"
#include "Unit.h"
#include "UnitStats.h" // ApplyBaseStats, BaseStats max-health

namespace
{

Unit Soldier(int team, UnitType type, int tileX, int tileY)
{
    Unit u;
    u.type = type;
    u.teamID = team;
    u.health = 100.0f;
    u.attackPower = 20;
    u.attackRange = 64;
    u.cooldownTime = 0.1f;
    u.speed = 64.0f;
    u.sightRange = 512.0f;
    u.damageType = DamageType::KINETIC;
    u.armorType = ArmorType::STEEL;
    u.position = cc::ToRaylib(cc::TileToWorld(tileX, tileY));
    return u;
}

Entity AddUnit(Registry &registry, const Unit &unit)
{
    const Entity id = registry.Create();
    registry.Add(id, unit);
    return id;
}

void StepUnits(Registry &registry, const TileMap &map, int frames)
{
    for (int i = 0; i < frames; ++i)
    {
        registry.Each<Unit>([&](Entity id, Unit &unit) {
            (void)unit;
            UpdateUnit(id, registry, map, 1.0f / 60.0f);
        });
    }
}

} // namespace

void RunOrdersTests()
{
    // --- attack-move engages on contact, then resumes the march ---
    {
        Registry registry;
        TileMap map(20, 15);
        const Entity seekerId =
            AddUnit(registry, Soldier(0, UnitType::Infantry, 2, 2));
        Unit victim = Soldier(1, UnitType::Infantry, 5, 2);
        victim.health = 30.0f;
        victim.attackPower = 0; // passive: never fights back
        const Entity victimId = AddUnit(registry, victim);
        Unit *seeker = registry.Get<Unit>(seekerId);
        IssueAttackMoveOrder(*seeker, map, cc::ToRaylib(cc::TileToWorld(10, 2)));
        CC_CHECK(seeker->attackMove);
        bool engaged = false;
        for (int i = 0; i < 1200; ++i)
        {
            StepUnits(registry, map, 1);
            if (registry.Get<Unit>(seekerId)->target != kInvalidEntity ||
                registry.Get<Unit>(seekerId)->state == UnitState::Attacking)
            {
                engaged = true;
            }
            if (!registry.IsAlive(victimId) || registry.Get<Unit>(victimId) == nullptr)
            {
                break;
            }
            const Unit *v = registry.Get<Unit>(victimId);
            if (v != nullptr && v->health <= 0.0f)
            {
                break;
            }
        }
        CC_CHECK(engaged); // made contact instead of walking past
        // Finish the victim (destroyed would clear it; health check suffices).
        for (int i = 0; i < 600; ++i)
        {
            StepUnits(registry, map, 1);
            const Unit *v = registry.Get<Unit>(victimId);
            if (v == nullptr || v->health <= 0.0f)
            {
                break;
            }
        }
        const Unit *v = registry.Get<Unit>(victimId);
        CC_CHECK(v == nullptr || v->health <= 0.0f);
        // March resumes to the recorded destination.
        for (int i = 0; i < 1200; ++i)
        {
            StepUnits(registry, map, 1);
            const Unit *s = registry.Get<Unit>(seekerId);
            if (!s->hasMoveOrder && !s->hasPath)
            {
                break;
            }
        }
        const Unit *s = registry.Get<Unit>(seekerId);
        CC_CHECK(!s->hasMoveOrder && !s->hasPath);
        CC_CHECK(cc::WorldToTile(cc::ToGlm(s->position)) == cc::IVec2(10, 2));
    }

    // --- attack-move with no contact walks like a plain order ---
    {
        Registry registry;
        TileMap map(20, 15);
        const Entity id = AddUnit(registry, Soldier(0, UnitType::Infantry, 2, 2));
        Unit *u = registry.Get<Unit>(id);
        IssueAttackMoveOrder(*u, map, cc::ToRaylib(cc::TileToWorld(6, 2)));
        StepUnits(registry, map, 600);
        const Unit *after = registry.Get<Unit>(id);
        CC_CHECK(!after->hasMoveOrder && !after->hasPath);
        CC_CHECK(cc::WorldToTile(cc::ToGlm(after->position)) == cc::IVec2(6, 2));
    }

    // --- Hold: no chase out of range, fires in range ---
    {
        Registry registry;
        TileMap map(20, 15);
        const Entity holderId = AddUnit(registry, Soldier(0, UnitType::Infantry, 2, 2));
        Unit *holder = registry.Get<Unit>(holderId);
        SetStance(*holder, Stance::Hold);
        Unit foe = Soldier(1, UnitType::Infantry, 5, 2);
        foe.attackPower = 0;
        const Entity foeId = AddUnit(registry, foe);
        StepUnits(registry, map, 120);
        const Unit *h = registry.Get<Unit>(holderId);
        CC_CHECK(h->target == kInvalidEntity); // never chased
        CC_CHECK(cc::WorldToTile(cc::ToGlm(h->position)) == cc::IVec2(2, 2));
        // Walk the foe into range: Hold opens fire without moving.
        registry.Get<Unit>(foeId)->position = cc::ToRaylib(cc::TileToWorld(3, 2));
        StepUnits(registry, map, 120);
        CC_CHECK(registry.Get<Unit>(foeId)->health < 100.0f);
        CC_CHECK(cc::WorldToTile(cc::ToGlm(registry.Get<Unit>(holderId)->position)) ==
                 cc::IVec2(2, 2));
    }

    // --- Patrol loops its legs while idle ---
    {
        Registry registry;
        TileMap map(20, 15);
        const Entity id = AddUnit(registry, Soldier(0, UnitType::Infantry, 2, 2));
        Unit *u = registry.Get<Unit>(id);
        IssuePatrolOrder(*u, map, cc::ToRaylib(cc::TileToWorld(2, 2)),
                         cc::ToRaylib(cc::TileToWorld(6, 2)));
        CC_CHECK(u->stance == Stance::Patrol && u->hasPatrol);
        // Walk to B (bounded): arrival clears the leg order...
        bool arrived = false;
        for (int i = 0; i < 900; ++i)
        {
            StepUnits(registry, map, 1);
            const Unit *p = registry.Get<Unit>(id);
            if (!p->hasMoveOrder && !p->hasPath)
            {
                arrived = true;
                break;
            }
        }
        CC_CHECK(arrived);
        CC_CHECK(cc::WorldToTile(cc::ToGlm(registry.Get<Unit>(id)->position)) == cc::IVec2(6, 2));
        // ...and the next idle tick issues the return leg.
        StepUnits(registry, map, 5);
        const Unit *back = registry.Get<Unit>(id);
        CC_CHECK(back->hasMoveOrder || back->hasPath);
        // Leaving patrol drops the route.
        SetStance(*registry.Get<Unit>(id), Stance::Guard);
        CC_CHECK(!registry.Get<Unit>(id)->hasPatrol);
    }

    // --- repair heals a damaged vehicle, then completes ---
    {
        Registry registry;
        TileMap map(20, 15);
        Unit tank = Soldier(0, UnitType::LightTank, 2, 2);
        ApplyBaseStats(tank);
        tank.teamID = 0;
        tank.health = BaseStats(UnitType::LightTank).health / 2.0f;
        const Entity tankId = AddUnit(registry, tank);
        const Entity engId = AddUnit(registry, Soldier(0, UnitType::Engineer, 2, 2));
        Unit *eng = registry.Get<Unit>(engId);
        eng->speed = 64.0f;
        IssueRepairOrder(*eng, tankId);
        CC_CHECK(eng->hasRepairOrder);
        StepUnits(registry, map, 3600);
        const Unit *fixed = registry.Get<Unit>(tankId);
        CC_CHECK(fixed->health == BaseStats(UnitType::LightTank).health);
        CC_CHECK(!registry.Get<Unit>(engId)->hasRepairOrder); // job done, order clears
    }

    // --- repair approaches from range ---
    {
        Registry registry;
        TileMap map(20, 15);
        Unit tank = Soldier(0, UnitType::LightTank, 8, 2);
        ApplyBaseStats(tank);
        tank.teamID = 0;
        tank.health = 10.0f;
        tank.attackPower = 0;
        const Entity tankId = AddUnit(registry, tank);
        const Entity engId = AddUnit(registry, Soldier(0, UnitType::Engineer, 2, 2));
        Unit *eng = registry.Get<Unit>(engId);
        eng->speed = 64.0f;
        eng->attackPower = 0;
        IssueRepairOrder(*eng, tankId);
        StepUnits(registry, map, 120);
        // Six tiles out: the engineer must be walking the approach, not idle.
        const Unit *e = registry.Get<Unit>(engId);
        CC_CHECK(e->hasMoveOrder || e->hasPath || e->state == UnitState::Moving);
        StepUnits(registry, map, 3600);
        CC_CHECK(registry.Get<Unit>(tankId)->health == BaseStats(UnitType::LightTank).health);
    }

    // --- repair rejects the invalid ---
    {
        Registry registry;
        TileMap map(20, 15);
        Unit tank = Soldier(0, UnitType::LightTank, 2, 2);
        ApplyBaseStats(tank);
        tank.teamID = 0;
        tank.health = 10.0f;
        const Entity tankId = AddUnit(registry, tank);
        // Non-engineer issuer: no-op.
        const Entity gruntId = AddUnit(registry, Soldier(0, UnitType::Infantry, 3, 2));
        IssueRepairOrder(*registry.Get<Unit>(gruntId), tankId);
        CC_CHECK(!registry.Get<Unit>(gruntId)->hasRepairOrder);
        // Enemy patient: order dies on the first tick.
        const Entity spyId = AddUnit(registry, Soldier(0, UnitType::Engineer, 3, 2));
        Unit foe = Soldier(1, UnitType::LightTank, 4, 2);
        ApplyBaseStats(foe);
        foe.teamID = 1;
        foe.health = 10.0f;
        const Entity foeId = AddUnit(registry, foe);
        IssueRepairOrder(*registry.Get<Unit>(spyId), foeId);
        StepUnits(registry, map, 5);
        CC_CHECK(!registry.Get<Unit>(spyId)->hasRepairOrder);
        // Healthy patient: nothing to fix.
        Unit whole = Soldier(0, UnitType::LightTank, 5, 2);
        ApplyBaseStats(whole);
        whole.teamID = 0;
        const Entity wholeId = AddUnit(registry, whole);
        IssueRepairOrder(*registry.Get<Unit>(spyId), wholeId);
        StepUnits(registry, map, 5);
        CC_CHECK(!registry.Get<Unit>(spyId)->hasRepairOrder);
        // Flesh (infantry) is not repairable.
        const Entity mateId = AddUnit(registry, Soldier(0, UnitType::Infantry, 6, 2));
        registry.Get<Unit>(mateId)->health = 10.0f;
        IssueRepairOrder(*registry.Get<Unit>(spyId), mateId);
        StepUnits(registry, map, 5);
        CC_CHECK(!registry.Get<Unit>(spyId)->hasRepairOrder);
    }

    // --- repair heals damaged buildings ---
    {
        Registry registry;
        TileMap map(20, 15);
        const Entity baseId =
            PlaceBuilding(registry, map, BuildingType::Base, 0, 5, 5);
        CC_CHECK(baseId != kInvalidEntity);
        CC_CHECK(registry.Get<Building>(baseId)->health == 600.0f);
        CC_CHECK(BuildingMaxHealth(BuildingType::ResourceDepot) == 250.0f);
        CC_CHECK(BuildingMaxHealth(BuildingType::Factory) == 500.0f);
        registry.Get<Building>(baseId)->health = 100.0f; // battle damage (simulated)
        const Entity engId = AddUnit(registry, Soldier(0, UnitType::Engineer, 5, 4));
        Unit *eng = registry.Get<Unit>(engId);
        eng->speed = 64.0f;
        IssueRepairOrder(*eng, baseId);
        StepUnits(registry, map, 3600);
        CC_CHECK(registry.Get<Building>(baseId)->health == 600.0f);
        CC_CHECK(!registry.Get<Unit>(engId)->hasRepairOrder);
    }
}
