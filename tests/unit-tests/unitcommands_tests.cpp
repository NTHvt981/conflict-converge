// Unit tests for selection commands shared by hotkeys and HUD buttons.

#include "test_harness.h"

#include "Building.h"
#include "Extensions.h"
#include "TileMap.h"
#include "Unit.h"
#include "UnitCommands.h"
#include "UnitStats.h"

namespace
{

Entity AddCommandUnit(Registry &registry, UnitType type, bool selected)
{
    const Entity id = registry.Create();
    Unit unit;
    unit.type = type;
    ApplyBaseStats(unit);
    unit.health = BaseStats(type).health;
    unit.isSelected = selected;
    registry.Add(id, unit);
    registry.Add(id, Orders{});
    registry.Add(id, Mover{});
    registry.Add(id, CombatState{});
    return id;
}

} // namespace

void RunUnitCommandsTests()
{
    // --- stance applies to every selected unit ---
    {
        Registry registry;
        const Entity a = AddCommandUnit(registry, UnitType::RifleInfantry, true);
        const Entity b = AddCommandUnit(registry, UnitType::RifleInfantry, true);
        SetSelectionStance(registry, Stance::Hold);
        CC_CHECK(FindOrders(registry, a)->stance == Stance::Hold);
        CC_CHECK(FindOrders(registry, b)->stance == Stance::Hold);
    }

    // --- halt clears movement, combat, and queued orders ---
    {
        Registry registry;
        TileMap map(20, 15);
        OccupancyGrid occ(20, 15);
        const Entity id =
            AddCommandUnit(registry, UnitType::RifleInfantry, true);
        IssueSelectionAttackMove(registry, map, &occ, { 640.0f, 640.0f }, false);
        GetOrders(registry, id).autoRetreat = true;
        HaltSelection(registry);
        const Orders *orders = FindOrders(registry, id);
        CC_CHECK(!orders->attackMove && orders->orderQueue.empty());
        CC_CHECK(orders->autoRetreat); // halt keeps the retreat stance flag
        CC_CHECK(registry.Get<Unit>(id)->state == UnitState::Idle);
        CC_CHECK(FindMover(registry, id)->speedCapPixelsPerSec == -1.0f);
    }

    // --- auto-retreat toggles as a group ---
    {
        Registry registry;
        const Entity a = AddCommandUnit(registry, UnitType::RifleInfantry, true);
        const Entity b = AddCommandUnit(registry, UnitType::RifleInfantry, true);
        ToggleSelectionAutoRetreat(registry);
        CC_CHECK(FindOrders(registry, a)->autoRetreat);
        CC_CHECK(FindOrders(registry, b)->autoRetreat);
        ToggleSelectionAutoRetreat(registry);
        CC_CHECK(!FindOrders(registry, a)->autoRetreat);
    }

    // --- attack-move and patrol arm the selection, skipping the unselected ---
    {
        Registry registry;
        TileMap map(20, 15);
        OccupancyGrid occ(20, 15);
        const Entity picked = AddCommandUnit(registry, UnitType::RifleInfantry, true);
        const Entity parked = AddCommandUnit(registry, UnitType::RifleInfantry, false);
        CC_CHECK(IssueSelectionAttackMove(registry, map, &occ, { 320.0f, 320.0f }, false) == 1);
        CC_CHECK(FindOrders(registry, picked)->attackMove);
        CC_CHECK(!FindOrders(registry, parked)->attackMove);
        CC_CHECK(IssueSelectionPatrol(registry, map, &occ, { 320.0f, 320.0f }, false) == 1);
        CC_CHECK(FindOrders(registry, picked)->hasPatrol);
        CC_CHECK(!FindOrders(registry, parked)->hasPatrol);
    }

    // --- repair arms engineers on damaged targets, nobody else ---
    {
        Registry registry;
        TileMap map(20, 15);
        OccupancyGrid occ(20, 15);
        const Entity engineer = AddCommandUnit(registry, UnitType::Engineer, true);
        const Entity rifle = AddCommandUnit(registry, UnitType::RifleInfantry, true);
        const Entity patient = AddCommandUnit(registry, UnitType::LightTank, false);
        registry.Get<Unit>(patient)->health = 10.0f;
        CC_CHECK(IssueSelectionRepair(registry, map, &occ, patient, false) == 1);
        CC_CHECK(FindOrders(registry, engineer)->hasRepairOrder);
        CC_CHECK(FindOrders(registry, engineer)->repairTarget == patient);
        CC_CHECK(!FindOrders(registry, rifle)->hasRepairOrder);
        CC_CHECK(IssueSelectionRepair(registry, map, &occ, rifle, false) == 0);
    }

    // --- heal arms medics on wounded flesh, nobody else ---
    {
        Registry registry;
        TileMap map(20, 15);
        OccupancyGrid occ(20, 15);
        const Entity medic = AddCommandUnit(registry, UnitType::Medic, true);
        const Entity wounded = AddCommandUnit(registry, UnitType::RifleInfantry, false);
        registry.Get<Unit>(wounded)->health = 10.0f;
        CC_CHECK(IssueSelectionHeal(registry, map, &occ, wounded, false) == 1);
        CC_CHECK(FindOrders(registry, medic)->repairTarget == wounded);
        const Entity tank = AddCommandUnit(registry, UnitType::LightTank, false);
        CC_CHECK(IssueSelectionHeal(registry, map, &occ, tank, false) == 0);
    }
}

namespace
{

void StepCommandUnits(Registry &registry, TileMap &map, int frames)
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

void RunAutoRepairTests()
{
    // --- toggle flips every selected engineer as a group ---
    {
        Registry registry;
        const Entity a = AddCommandUnit(registry, UnitType::Engineer, true);
        const Entity b = AddCommandUnit(registry, UnitType::Engineer, true);
        AddCommandUnit(registry, UnitType::RifleInfantry, true);
        ToggleSelectionAutoRepair(registry);
        CC_CHECK(FindOrders(registry, a)->autoRepair);
        CC_CHECK(FindOrders(registry, b)->autoRepair);
        ToggleSelectionAutoRepair(registry);
        CC_CHECK(!FindOrders(registry, a)->autoRepair);
        CC_CHECK(!FindOrders(registry, b)->autoRepair);
    }

    // --- toggled engineer acquires and heals a nearby damaged vehicle ---
    {
        Registry registry;
        TileMap map(20, 15);
        const Entity engId = AddCommandUnit(registry, UnitType::Engineer, false);
        registry.Get<Unit>(engId)->speed = 64.0f;
        const Entity tankId = AddCommandUnit(registry, UnitType::LightTank, false);
        registry.Get<Unit>(tankId)->health = 10.0f;
        GetOrders(registry, engId).autoRepair = true;
        StepCommandUnits(registry, map, 5);
        CC_CHECK(GetOrders(registry, engId).hasRepairOrder);
        StepCommandUnits(registry, map, 3600);
        CC_CHECK(registry.Get<Unit>(tankId)->health == BaseStats(UnitType::LightTank).health);
        CC_CHECK(!GetOrders(registry, engId).hasRepairOrder);
    }

    // --- toggled engineer acquires and heals a nearby damaged building ---
    {
        Registry registry;
        TileMap map(20, 15);
        const Entity baseId = PlaceBuilding(registry, map, BuildingType::Base, 0, 1, 1);
        CC_CHECK(baseId != kInvalidEntity);
        UpdateBuildingConstruction(registry, 20.0f);
        registry.Get<Building>(baseId)->health = 100.0f;
        const Entity engId = AddCommandUnit(registry, UnitType::Engineer, false);
        registry.Get<Unit>(engId)->speed = 64.0f;
        GetOrders(registry, engId).autoRepair = true;
        StepCommandUnits(registry, map, 3600);
        CC_CHECK(registry.Get<Building>(baseId)->health == 400.0f);
    }

    // --- untoggled, busy, and distant engineers acquire nothing ---
    {
        Registry registry;
        TileMap map(20, 15);
        const Entity offId = AddCommandUnit(registry, UnitType::Engineer, false);
        const Entity tankId = AddCommandUnit(registry, UnitType::LightTank, false);
        registry.Get<Unit>(tankId)->health = 10.0f;
        StepCommandUnits(registry, map, 30);
        CC_CHECK(!GetOrders(registry, offId).hasRepairOrder);

        const Entity busyId = AddCommandUnit(registry, UnitType::Engineer, false);
        GetOrders(registry, busyId).autoRepair = true;
        GetMover(registry, busyId).moveTarget = { 640.0f, 640.0f }; // marching: no acquire
        GetMover(registry, busyId).hasMoveOrder = true;
        StepCommandUnits(registry, map, 30);
        CC_CHECK(!GetOrders(registry, busyId).hasRepairOrder);

        const Entity farId = AddCommandUnit(registry, UnitType::Engineer, false);
        registry.Get<Unit>(farId)->position = { 640.0f, 640.0f };
        GetOrders(registry, farId).autoRepair = true;
        StepCommandUnits(registry, map, 30);
        CC_CHECK(!GetOrders(registry, farId).hasRepairOrder);
    }
}
