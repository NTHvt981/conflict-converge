// Unit tests for QoL area repair (candidate sweep + greedy
// nearest-unclaimed assignment). Game.cpp's drag gesture itself is input
// handling (untested per convention); everything it calls is covered here.

#include "test_harness.h"

#include "Building.h"
#include "Extensions.h"
#include "Targeting.h"
#include "TileMap.h"
#include "Unit.h"
#include "UnitStats.h"

namespace
{

Entity SpawnUnit(Registry &registry, UnitType type, int team, int tileX, int tileY,
                 float hpFrac)
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

Entity SpawnBuilding(Registry &registry, TileMap &map, BuildingType type, int team,
                     int tileX, int tileY, float hpFrac)
{
    const Entity entity = PlaceBuilding(registry, map, type, team, tileX, tileY);
    if (entity != kInvalidEntity)
    {
        Building *building = registry.Get<Building>(entity);
        building->state = BuildingState::Operational; // repair needs a standing structure
        building->health = building->maxHealth * hpFrac;
    }
    return entity;
}

Rectangle WorldRect(float x, float y, float w, float h)
{
    return { x, y, w, h };
}

} // namespace

void RunAreaRepairTests()
{
    // --- sweep collects damaged supportables, skips everything else ---
    // Vehicles + buildings are engineer work, wounded flesh is medic work;
    // both land in the shared pool (assignment filters per unit type).
    {
        Registry registry;
        TileMap map(20, 15);
        const Entity hurtTank = SpawnUnit(registry, UnitType::LightTank, 0, 2, 2, 0.5f);
        const Entity wholeTank = SpawnUnit(registry, UnitType::LightTank, 0, 3, 2, 1.0f);
        const Entity hurtFlesh = SpawnUnit(registry, UnitType::RifleInfantry, 0, 2, 3, 0.5f);
        const Entity foeTank = SpawnUnit(registry, UnitType::LightTank, 1, 4, 2, 0.5f);
        const Entity farTank = SpawnUnit(registry, UnitType::LightTank, 0, 15, 12, 0.5f);
        const Entity hurtBase = SpawnBuilding(registry, map, BuildingType::Base, 0, 6, 2, 0.5f);
        const Entity wholeDepot =
            SpawnBuilding(registry, map, BuildingType::ResourceDepot, 0, 8, 2, 1.0f);
        const Entity deadBase = SpawnBuilding(registry, map, BuildingType::Base, 0, 10, 2, 0.5f);
        registry.Get<Building>(deadBase)->state = BuildingState::Destroyed;
        CC_CHECK(hurtBase != kInvalidEntity && wholeDepot != kInvalidEntity &&
                 deadBase != kInvalidEntity);

        // Zone covers tiles x 0..11, y 0..5 (everything above except farTank).
        std::vector<Entity> candidates;
        CollectAreaRepairCandidates(registry, WorldRect(0.0f, 0.0f, 12.0f * 64.0f,
                                                        6.0f * 64.0f),
                                    0, candidates);
        CC_CHECK(candidates.size() == 3);
        bool hasTank = false;
        bool hasBase = false;
        bool hasFlesh = false;
        for (const Entity id : candidates)
        {
            hasTank = hasTank || id == hurtTank;
            hasBase = hasBase || id == hurtBase;
            hasFlesh = hasFlesh || id == hurtFlesh;
        }
        CC_CHECK(hasTank && hasBase && hasFlesh);
        (void)wholeTank;
        (void)foeTank;
        (void)farTank;
        (void)wholeDepot;
        (void)deadBase;
    }

    // --- greedy: each Engineer takes its nearest unclaimed target ---
    {
        Registry registry;
        TileMap map(20, 15);
        const Entity engA = SpawnUnit(registry, UnitType::Engineer, 0, 1, 1, 1.0f);
        const Entity engB = SpawnUnit(registry, UnitType::Engineer, 0, 10, 1, 1.0f);
        const Entity targetA = SpawnUnit(registry, UnitType::LightTank, 0, 2, 1, 0.5f);
        const Entity targetB = SpawnUnit(registry, UnitType::LightTank, 0, 9, 1, 0.5f);
        const std::vector<Entity> engineers = { engA, engB };
        const std::vector<Entity> candidates = { targetA, targetB };

        std::vector<RepairAssignment> jobs;
        CC_CHECK(AssignAreaRepair(registry, engineers, candidates, jobs) == 2);
        CC_CHECK(jobs.size() == 2);
        CC_CHECK(jobs[0].engineer == engA && jobs[0].target == targetA);
        CC_CHECK(jobs[1].engineer == engB && jobs[1].target == targetB);
    }

    // --- no double-assignment: one target, two Engineers ---
    {
        Registry registry;
        TileMap map(20, 15);
        const Entity engA = SpawnUnit(registry, UnitType::Engineer, 0, 1, 1, 1.0f);
        const Entity engB = SpawnUnit(registry, UnitType::Engineer, 0, 2, 1, 1.0f);
        const Entity lone = SpawnUnit(registry, UnitType::LightTank, 0, 1, 2, 0.5f);
        const std::vector<Entity> engineers = { engA, engB };
        const std::vector<Entity> candidates = { lone };

        std::vector<RepairAssignment> jobs;
        CC_CHECK(AssignAreaRepair(registry, engineers, candidates, jobs) == 1);
        CC_CHECK(jobs[0].engineer == engA); // first Engineer wins the claim
        CC_CHECK(jobs[0].target == lone);
    }

    // --- leftover targets wait: one Engineer, three damaged ---
    {
        Registry registry;
        TileMap map(20, 15);
        const Entity eng = SpawnUnit(registry, UnitType::Engineer, 0, 1, 1, 1.0f);
        const Entity near = SpawnUnit(registry, UnitType::LightTank, 0, 2, 1, 0.5f);
        const Entity mid = SpawnUnit(registry, UnitType::LightTank, 0, 5, 1, 0.5f);
        const Entity far = SpawnUnit(registry, UnitType::LightTank, 0, 9, 1, 0.5f);
        const std::vector<Entity> engineers = { eng };
        const std::vector<Entity> candidates = { far, mid, near }; // shuffled input

        std::vector<RepairAssignment> jobs;
        CC_CHECK(AssignAreaRepair(registry, engineers, candidates, jobs) == 1);
        CC_CHECK(jobs[0].target == near); // nearest regardless of input order
        (void)mid;
        (void)far;
    }

    // --- mixed pool: buildings compete with units on distance alone ---
    {
        Registry registry;
        TileMap map(20, 15);
        const Entity eng = SpawnUnit(registry, UnitType::Engineer, 0, 1, 5, 1.0f);
        const Entity tank = SpawnUnit(registry, UnitType::LightTank, 0, 8, 5, 0.5f);
        const Entity base = SpawnBuilding(registry, map, BuildingType::Base, 0, 2, 5, 0.5f);
        CC_CHECK(base != kInvalidEntity);
        const std::vector<Entity> engineers = { eng };
        const std::vector<Entity> candidates = { tank, base };

        std::vector<RepairAssignment> jobs;
        CC_CHECK(AssignAreaRepair(registry, engineers, candidates, jobs) == 1);
        CC_CHECK(jobs[0].target == base); // adjacent base beats the far tank
    }

    // --- empty inputs assign nothing (no crash) ---
    {
        Registry registry;
        TileMap map(20, 15);
        const std::vector<Entity> empty;
        std::vector<RepairAssignment> jobs;
        CC_CHECK(AssignAreaRepair(registry, empty, empty, jobs) == 0);
        CC_CHECK(jobs.empty());
    }

    // --- medic heal aim: same-team damaged flesh only ---
    {
        Registry registry;
        TileMap map(20, 15);
        const Entity medic = SpawnUnit(registry, UnitType::Medic, 0, 1, 1, 1.0f);
        const Entity eng = SpawnUnit(registry, UnitType::Engineer, 0, 1, 2, 1.0f);
        const Entity hurt = SpawnUnit(registry, UnitType::RifleInfantry, 0, 2, 1, 0.5f);
        const Entity whole = SpawnUnit(registry, UnitType::RifleInfantry, 0, 3, 1, 1.0f);
        const Entity foe = SpawnUnit(registry, UnitType::RifleInfantry, 1, 2, 2, 0.5f);
        const Entity tank = SpawnUnit(registry, UnitType::LightTank, 0, 4, 1, 0.5f);
        const Entity depot = SpawnBuilding(registry, map, BuildingType::ResourceDepot, 0, 6, 1, 0.5f);
        const Unit *med = registry.Get<Unit>(medic);
        const Unit *engineer = registry.Get<Unit>(eng);
        CC_CHECK(med != nullptr && engineer != nullptr && depot != kInvalidEntity);
        CC_CHECK(CanHealTarget(registry, *med, hurt));
        CC_CHECK(!CanHealTarget(registry, *med, whole)); // full health
        CC_CHECK(!CanHealTarget(registry, *med, foe));   // wrong team
        CC_CHECK(!CanHealTarget(registry, *med, tank));  // vehicles are engineer work
        CC_CHECK(!CanHealTarget(registry, *med, depot)); // buildings are engineer work
        CC_CHECK(!CanHealTarget(registry, *med, kInvalidEntity));
        CC_CHECK(!CanHealTarget(registry, *engineer, hurt)); // engineers cannot heal
        CC_CHECK(!CanRepairTarget(registry, *med, tank));    // medics cannot repair
    }

    // --- heal execution: channeled restore, clamped, order completes ---
    {
        Registry registry;
        TileMap map(20, 15);
        const Entity medic = SpawnUnit(registry, UnitType::Medic, 0, 1, 1, 1.0f);
        const Entity hurt = SpawnUnit(registry, UnitType::RifleInfantry, 0, 2, 1, 0.5f);
        Unit *med = registry.Get<Unit>(medic);
        Orders &medOrders = GetOrders(registry, medic);
        CC_CHECK(med != nullptr);
        IssueHealOrder(*med, medOrders, GetMover(registry, medic), hurt);
        CC_CHECK(medOrders.hasRepairOrder && medOrders.repairTarget == hurt);
        const float before = registry.Get<Unit>(hurt)->health;
        UpdateUnit(medic, registry, map, 1.0f);
        CC_CHECK(registry.Get<Unit>(hurt)->health > before);
        CC_CHECK(med->state == UnitState::Idle);
        for (int i = 0; i < 10 && medOrders.hasRepairOrder; ++i)
        {
            UpdateUnit(medic, registry, map, 1.0f);
        }
        CC_CHECK(!medOrders.hasRepairOrder); // full health drops the order
        CC_CHECK(registry.Get<Unit>(hurt)->health ==
                 BaseStats(UnitType::RifleInfantry).health); // clamped, not overhealed
    }

    // --- area assign pairs medics with flesh: never self, never vehicles ---
    {
        Registry registry;
        const Entity medic = SpawnUnit(registry, UnitType::Medic, 0, 1, 1, 0.5f);
        const Entity flesh = SpawnUnit(registry, UnitType::RifleInfantry, 0, 5, 5, 0.5f);
        const Entity tank = SpawnUnit(registry, UnitType::LightTank, 0, 2, 1, 0.5f);
        std::vector<RepairAssignment> jobs;
        CC_CHECK(AssignAreaRepair(registry, { medic }, { medic, tank, flesh }, jobs) == 1);
        CC_CHECK(jobs[0].engineer == medic && jobs[0].target == flesh);
    }

    // --- support types cannot attack: no acquire, no engage, no damage ---
    {
        Registry registry;
        TileMap map(20, 15);
        const Entity medic = SpawnUnit(registry, UnitType::Medic, 0, 1, 1, 1.0f);
        const Entity eng = SpawnUnit(registry, UnitType::Engineer, 0, 1, 3, 1.0f);
        const Entity foe = SpawnUnit(registry, UnitType::RifleInfantry, 1, 3, 1, 1.0f);
        CC_CHECK(AcquireTarget(registry, medic, nullptr, nullptr) == kInvalidEntity);
        CC_CHECK(AcquireTarget(registry, eng, nullptr, nullptr) == kInvalidEntity);
        const float foeMax = BaseStats(UnitType::RifleInfantry).health;
        for (int i = 0; i < 180; ++i) // 3s of Guard-stance updates in sight range
        {
            UpdateUnit(medic, registry, map, 1.0f / 60.0f);
            UpdateUnit(eng, registry, map, 1.0f / 60.0f);
        }
        CC_CHECK(registry.Get<Unit>(foe)->health == foeMax);
        CC_CHECK(registry.Get<Unit>(medic)->state != UnitState::Attacking);
        CC_CHECK(FindCombatState(registry, medic)->target == kInvalidEntity);
        CC_CHECK(registry.Get<Unit>(eng)->state != UnitState::Attacking);
        CC_CHECK(FindCombatState(registry, eng)->target == kInvalidEntity);
    }
}
