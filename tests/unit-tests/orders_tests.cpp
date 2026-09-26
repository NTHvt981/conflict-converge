// Unit tests for orders: attack-move, stances, patrol, repair (+ HP).

#include "test_harness.h"

#include "units/AICommander.h" // factory-gate test
#include "economy/Building.h" // PlaceBuilding, BuildingMaxHealth
#include "core/Event.h"    // commander event routing
#include "units/Extensions.h" // Orders pool (Unit split slice 1)
#include "world/FogOfWar.h" // structure acquisition under fog
#include "economy/Nodes.h"    // ResourceNodes for the commander fixture
#include "app/input/Selection.h" // G4 embarked pick/select guards
#include "units/Targeting.h" // AcquireBuildingTarget
#include "world/TileMap.h"
#include "units/Unit.h"
#include "units/UnitStats.h" // ApplyBaseStats, BaseStats max-health

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
    registry.Add(id, Orders{});
    registry.Add(id, Mover{});
    registry.Add(id, CombatState{});
    return id;
}

void StepUnits(Registry &registry, TileMap &map, int frames)
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
            AddUnit(registry, Soldier(0, UnitType::RifleInfantry, 2, 2));
        Unit victim = Soldier(1, UnitType::RifleInfantry, 5, 2);
        victim.health = 30.0f;
        victim.attackPower = 0; // passive: never fights back
        const Entity victimId = AddUnit(registry, victim);
        Unit *seeker = registry.Get<Unit>(seekerId);
        Orders &seekerOrders = GetOrders(registry, seekerId);
        Mover &seekerMover = GetMover(registry, seekerId);
        IssueAttackMoveOrder(*seeker, seekerOrders, seekerMover, map,
                             cc::ToRaylib(cc::TileToWorld(10, 2)));
        CC_CHECK(seekerOrders.attackMove);
        bool engaged = false;
        for (int i = 0; i < 1200; ++i)
        {
            StepUnits(registry, map, 1);
            if (FindCombatState(registry, seekerId)->target != kInvalidEntity ||
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
            const Mover *s = FindMover(registry, seekerId);
            if (s == nullptr || (!s->hasMoveOrder && !s->hasPath))
            {
                break;
            }
        }
        CC_CHECK(!FindMover(registry, seekerId)->hasMoveOrder &&
                 !FindMover(registry, seekerId)->hasPath);
        CC_CHECK(cc::WorldToTile(cc::ToGlm(registry.Get<Unit>(seekerId)->position)) ==
                 cc::IVec2(10, 2));
    }

    // --- attack-move with no contact walks like a plain order ---
    {
        Registry registry;
        TileMap map(20, 15);
        const Entity id = AddUnit(registry, Soldier(0, UnitType::RifleInfantry, 2, 2));
        Unit *u = registry.Get<Unit>(id);
        IssueAttackMoveOrder(*u, GetOrders(registry, id), GetMover(registry, id), map,
                             cc::ToRaylib(cc::TileToWorld(6, 2)));
        StepUnits(registry, map, 600);
        const Mover *after = FindMover(registry, id);
        CC_CHECK(!after->hasMoveOrder && !after->hasPath);
        CC_CHECK(cc::WorldToTile(cc::ToGlm(registry.Get<Unit>(id)->position)) == cc::IVec2(6, 2));
    }

    // --- Hold: no chase out of range, fires in range ---
    {
        Registry registry;
        TileMap map(20, 15);
        const Entity holderId = AddUnit(registry, Soldier(0, UnitType::RifleInfantry, 2, 2));
        Unit *holder = registry.Get<Unit>(holderId);
        SetStance(*holder, GetOrders(registry, holderId), GetCombatState(registry, holderId),
                  Stance::Hold);
        Unit foe = Soldier(1, UnitType::RifleInfantry, 5, 2);
        foe.attackPower = 0;
        const Entity foeId = AddUnit(registry, foe);
        StepUnits(registry, map, 120);
        const Unit *h = registry.Get<Unit>(holderId);
        CC_CHECK(FindCombatState(registry, holderId)->target == kInvalidEntity); // never chased
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
        const Entity id = AddUnit(registry, Soldier(0, UnitType::RifleInfantry, 2, 2));
        Unit *u = registry.Get<Unit>(id);
        Orders &orders = GetOrders(registry, id);
        Mover &mover = GetMover(registry, id);
        IssuePatrolOrder(*u, orders, mover, map, cc::ToRaylib(cc::TileToWorld(2, 2)),
                         cc::ToRaylib(cc::TileToWorld(6, 2)));
        CC_CHECK(orders.stance == Stance::Patrol && orders.hasPatrol);
        // Walk to B (bounded): arrival clears the leg order...
        bool arrived = false;
        for (int i = 0; i < 900; ++i)
        {
            StepUnits(registry, map, 1);
            const Mover *p = FindMover(registry, id);
            if (p == nullptr || (!p->hasMoveOrder && !p->hasPath))
            {
                arrived = true;
                break;
            }
        }
        CC_CHECK(arrived);
        CC_CHECK(cc::WorldToTile(cc::ToGlm(registry.Get<Unit>(id)->position)) == cc::IVec2(6, 2));
        // ...and the next idle tick issues the return leg.
        StepUnits(registry, map, 5);
        const Mover *back = FindMover(registry, id);
        CC_CHECK(back->hasMoveOrder || back->hasPath);
        // Leaving patrol drops the route.
        SetStance(*registry.Get<Unit>(id), GetOrders(registry, id),
                  GetCombatState(registry, id), Stance::Guard);
        CC_CHECK(!GetOrders(registry, id).hasPatrol);
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
        IssueRepairOrder(*eng, GetOrders(registry, engId), GetMover(registry, engId), tankId);
        CC_CHECK(GetOrders(registry, engId).hasRepairOrder);
        StepUnits(registry, map, 3600);
        const Unit *fixed = registry.Get<Unit>(tankId);
        CC_CHECK(fixed->health == BaseStats(UnitType::LightTank).health);
        CC_CHECK(!GetOrders(registry, engId).hasRepairOrder); // job done, order clears
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
        IssueRepairOrder(*eng, GetOrders(registry, engId), GetMover(registry, engId), tankId);
        StepUnits(registry, map, 120);
        // Six tiles out: the engineer must be walking the approach, not idle.
        const Unit *e = registry.Get<Unit>(engId);
        const Mover *em = FindMover(registry, engId);
        CC_CHECK(em->hasMoveOrder || em->hasPath || e->state == UnitState::Moving);
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
        const Entity gruntId = AddUnit(registry, Soldier(0, UnitType::RifleInfantry, 3, 2));
        IssueRepairOrder(*registry.Get<Unit>(gruntId), GetOrders(registry, gruntId),
                         GetMover(registry, gruntId), tankId);
        CC_CHECK(!GetOrders(registry, gruntId).hasRepairOrder);
        // Enemy patient: order dies on the first tick.
        const Entity spyId = AddUnit(registry, Soldier(0, UnitType::Engineer, 3, 2));
        Unit foe = Soldier(1, UnitType::LightTank, 4, 2);
        ApplyBaseStats(foe);
        foe.teamID = 1;
        foe.health = 10.0f;
        const Entity foeId = AddUnit(registry, foe);
        IssueRepairOrder(*registry.Get<Unit>(spyId), GetOrders(registry, spyId),
                         GetMover(registry, spyId), foeId);
        StepUnits(registry, map, 5);
        CC_CHECK(!GetOrders(registry, spyId).hasRepairOrder);
        // Healthy patient: nothing to fix.
        Unit whole = Soldier(0, UnitType::LightTank, 5, 2);
        ApplyBaseStats(whole);
        whole.teamID = 0;
        const Entity wholeId = AddUnit(registry, whole);
        IssueRepairOrder(*registry.Get<Unit>(spyId), GetOrders(registry, spyId),
                         GetMover(registry, spyId), wholeId);
        StepUnits(registry, map, 5);
        CC_CHECK(!GetOrders(registry, spyId).hasRepairOrder);
        // Flesh (infantry) is not repairable.
        const Entity mateId = AddUnit(registry, Soldier(0, UnitType::RifleInfantry, 6, 2));
        registry.Get<Unit>(mateId)->health = 10.0f;
        IssueRepairOrder(*registry.Get<Unit>(spyId), GetOrders(registry, spyId),
                         GetMover(registry, spyId), mateId);
        StepUnits(registry, map, 5);
        CC_CHECK(!GetOrders(registry, spyId).hasRepairOrder);
    }

    // --- repair heals damaged buildings ---
    {
        Registry registry;
        TileMap map(20, 15);
        const Entity baseId =
            PlaceBuilding(registry, map, BuildingType::Base, 0, 5, 5);
        CC_CHECK(baseId != kInvalidEntity);
        UpdateBuildingConstruction(registry, 20.0f); // Operational before wounding
        CC_CHECK(registry.Get<Building>(baseId)->health == 400.0f);
        CC_CHECK(BuildingMaxHealth(BuildingType::ResourceDepot) == 200.0f);
        CC_CHECK(BuildingMaxHealth(BuildingType::Factory) == 350.0f);
        registry.Get<Building>(baseId)->health = 100.0f; // battle damage (simulated)
        const Entity engId = AddUnit(registry, Soldier(0, UnitType::Engineer, 5, 4));
        Unit *eng = registry.Get<Unit>(engId);
        eng->speed = 64.0f;
        IssueRepairOrder(*eng, GetOrders(registry, engId), GetMover(registry, engId), baseId);
        StepUnits(registry, map, 3600);
        CC_CHECK(registry.Get<Building>(baseId)->health == 400.0f);
        CC_CHECK(!GetOrders(registry, engId).hasRepairOrder);
    }

    // --- structures are acquired like targets (nearest, hostile, standing) ---
    {
        Registry registry;
        TileMap map(20, 15);
        const Entity seekerId = AddUnit(registry, Soldier(0, UnitType::LightTank, 2, 2));
        Unit *seeker = registry.Get<Unit>(seekerId);
        seeker->sightRange = 512.0f;
        const Entity farBase =
            PlaceBuilding(registry, map, BuildingType::Base, 1, 7, 2);
        const Entity nearDepot =
            PlaceBuilding(registry, map, BuildingType::ResourceDepot, 1, 5, 2);
        const Entity ownFactory =
            PlaceBuilding(registry, map, BuildingType::Factory, 0, 2, 5);
        UpdateBuildingConstruction(registry, 20.0f); // targets must be Operational
        CC_CHECK(AcquireBuildingTarget(registry, seekerId, nullptr) == nearDepot);
        // Own structures are never targets.
        CC_CHECK(AcquireBuildingTarget(registry, seekerId, nullptr) != ownFactory);
        // Wrecks drop out: demolish the depot, the base becomes the target.
        DemolishBuilding(registry, map, nearDepot);
        CC_CHECK(AcquireBuildingTarget(registry, seekerId, nullptr) == farBase);
        // Fog hides structures from everyone but artillery.
        FogOfWar fog;
        fog.Resize(20, 15); // nothing recomputed: team 0 sees nothing
        CC_CHECK(AcquireBuildingTarget(registry, seekerId, &fog) == kInvalidEntity);
        seeker->type = UnitType::Artillery;
        CC_CHECK(AcquireBuildingTarget(registry, seekerId, &fog) == farBase);
    }

    // --- driver razes structures: depot falls, tiles free up ---
    {
        Registry registry;
        TileMap map(20, 15);
        const Entity depotId =
            PlaceBuilding(registry, map, BuildingType::ResourceDepot, 1, 5, 2);
        UpdateBuildingConstruction(registry, 20.0f); // raidable only when Operational
        const Entity raiderId = AddUnit(registry, Soldier(0, UnitType::LightTank, 3, 2));
        Unit *raider = registry.Get<Unit>(raiderId);
        raider->sightRange = 512.0f;
        StepUnits(registry, map, 3600);
        CC_CHECK(registry.Get<Building>(depotId) == nullptr); // demolished at zero HP
        CC_CHECK(map.Get({ 5, 2 }) == TerrainType::Grass);     // footprint restored
        CC_CHECK(FindCombatState(registry, raiderId)->target == kInvalidEntity);
    }

    // --- production dies with the factory; the queue stalls ---
    {
        Registry registry;
        TileMap map(20, 15);
        ResourceNodes nodes;
        EventDispatcher events;
        AICommander ai(registry, map, nodes, events, 1, AIDifficulty::Easy, { 10, 10 }, { 2, 2 });
        ai.SetupBase();
        UpdateBuildingConstruction(registry, 20.0f); // factory must be Operational
        CC_CHECK(ai.HasFactory());
        Entity factoryId = kInvalidEntity;
        registry.Each<Building>([&](Entity id, const Building &b) {
            if (b.teamID == 1 && b.type == BuildingType::Factory)
            {
                factoryId = id;
            }
        });
        CC_CHECK(factoryId != kInvalidEntity);
        CC_CHECK(DemolishBuilding(registry, map, factoryId));
        CC_CHECK(!ai.HasFactory());
        for (int i = 0; i < 300; ++i)
        {
            ai.Update(1.0f / 60.0f);
        }
        // Guard stands alone: no queue completions without a factory.
        CC_CHECK(ai.CombatUnitCount() == 1);
    }

    // --- G4 load: adjacent foot boards, carrier keeps the manifest ---
    {
        Registry registry;
        TileMap map(20, 15);
        const Entity carrierId = AddUnit(registry, Soldier(0, UnitType::IFV, 2, 2));
        Cargo cargo;
        cargo.capacity = 2;
        registry.Add(carrierId, cargo);
        const Entity riderId = AddUnit(registry, Soldier(0, UnitType::RifleInfantry, 3, 2));
        IssueLoadOrder(*registry.Get<Unit>(carrierId), GetOrders(registry, carrierId),
                       GetMover(registry, carrierId), riderId);
        StepUnits(registry, map, 5);
        CC_CHECK(registry.Has<EmbarkedOn>(riderId));
        CC_CHECK(registry.Get<EmbarkedOn>(riderId)->carrier == carrierId);
        CC_CHECK(registry.Get<Cargo>(carrierId)->passengers.size() == 1);
        CC_CHECK(!GetOrders(registry, carrierId).hasLoadOrder); // order consumed
    }

    // --- G4 load validation: enemies, vehicles, full, and carrier-less ---
    {
        Registry registry;
        TileMap map(20, 15);
        const Entity carrierId = AddUnit(registry, Soldier(0, UnitType::IFV, 2, 2));
        Cargo cargo;
        cargo.capacity = 1;
        registry.Add(carrierId, cargo);
        const Unit *carrier = registry.Get<Unit>(carrierId);
        const Entity foeId = AddUnit(registry, Soldier(1, UnitType::RifleInfantry, 2, 3));
        CC_CHECK(!CanLoadTarget(registry, carrierId, *carrier, foeId)); // enemy
        const Entity truckId = AddUnit(registry, Soldier(0, UnitType::LightTank, 2, 3));
        CC_CHECK(!CanLoadTarget(registry, carrierId, *carrier, truckId)); // vehicle
        const Entity firstId = AddUnit(registry, Soldier(0, UnitType::Engineer, 2, 3));
        CC_CHECK(CanLoadTarget(registry, carrierId, *carrier, firstId));
        CC_CHECK(BoardTransport(registry, carrierId, firstId));
        const Entity secondId = AddUnit(registry, Soldier(0, UnitType::Medic, 2, 3));
        CC_CHECK(!CanLoadTarget(registry, carrierId, *carrier, secondId)); // full
        CC_CHECK(!BoardTransport(registry, carrierId, secondId));
        const Entity bareId = AddUnit(registry, Soldier(0, UnitType::LightTank, 4, 4));
        CC_CHECK(!CanLoadTarget(registry, bareId, *registry.Get<Unit>(bareId),
                                firstId)); // no Cargo
    }

    // --- G4 unload: passengers land on free tiles, manifest clears ---
    {
        Registry registry;
        TileMap map(20, 15);
        const Entity carrierId = AddUnit(registry, Soldier(0, UnitType::IFV, 5, 5));
        Cargo cargo;
        cargo.capacity = 4;
        registry.Add(carrierId, cargo);
        const Entity riderId = AddUnit(registry, Soldier(0, UnitType::RifleInfantry, 5, 5));
        CC_CHECK(BoardTransport(registry, carrierId, riderId));
        IssueUnloadOrder(*registry.Get<Unit>(carrierId), GetOrders(registry, carrierId),
                         GetMover(registry, carrierId), map, cc::ToRaylib(cc::TileToWorld(8, 8)));
        StepUnits(registry, map, 600); // drives there, then drops
        CC_CHECK(!registry.Has<EmbarkedOn>(riderId));
        CC_CHECK(registry.Get<Cargo>(carrierId)->passengers.empty());
        CC_CHECK(!GetOrders(registry, carrierId).hasUnloadOrder);
        CC_CHECK(registry.IsAlive(riderId));
    }

    // --- G4 embarked: untargetable, unpickable, unselectable ---
    {
        Registry registry;
        TileMap map(20, 15);
        const Entity carrierId = AddUnit(registry, Soldier(0, UnitType::IFV, 2, 2));
        Cargo cargo;
        cargo.capacity = 2;
        registry.Add(carrierId, cargo);
        const Entity riderId = AddUnit(registry, Soldier(0, UnitType::RifleInfantry, 2, 2));
        CC_CHECK(BoardTransport(registry, carrierId, riderId));
        const Entity seekerId = AddUnit(registry, Soldier(1, UnitType::RifleInfantry, 2, 3));
        CC_CHECK(AcquireTarget(registry, seekerId, nullptr) != riderId); // untargetable
        CC_CHECK(PickUnitAt(registry, registry.Get<Unit>(riderId)->position) !=
                 riderId); // unpickable
        CC_CHECK(SelectInRect(registry, { 0.0f, 0.0f, 512.0f, 512.0f }, false) == 2);
        CC_CHECK(!registry.Get<Unit>(riderId)->isSelected); // unselectable
    }
}
