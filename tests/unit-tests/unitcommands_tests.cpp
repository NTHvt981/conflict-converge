// Unit tests for selection commands shared by hotkeys and HUD buttons.

#include "test_harness.h"

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
