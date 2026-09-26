// Unit tests for HUD text builders (the raygui Draw* wrappers need
// a window and are exercised live in main.cpp, not here).

#include "test_harness.h"

#include "economy/Building.h"
#include "units/Extensions.h"
#include "app/ui/Hud.h"
#include "units/UnitStats.h"

void RunHudAbilityTests();

void RunHudTests()
{
    // --- unit type names cover all 9 types ---
    CC_CHECK(std::string(UnitTypeName(UnitType::RifleInfantry)) == "Rifle Infantry");
    CC_CHECK(std::string(UnitTypeName(UnitType::AntiArmorInfantry)) == "Anti-Armor");
    CC_CHECK(std::string(UnitTypeName(UnitType::Engineer)) == "Engineer");
    CC_CHECK(std::string(UnitTypeName(UnitType::IFV)) == "IFV");
    CC_CHECK(std::string(UnitTypeName(UnitType::Artillery)) == "Artillery");
    CC_CHECK(std::string(UnitTypeName(UnitType::LightTank)) == "Light Tank");
    CC_CHECK(std::string(UnitTypeName(UnitType::HeavyTank)) == "Heavy Tank");
    CC_CHECK(std::string(UnitTypeName(UnitType::PrototypeInfantry)) == "Prototype Infantry");
    CC_CHECK(std::string(UnitTypeName(UnitType::Medic)) == "Medic");

    // --- resource formatting ---
    ResourceSystem resources;
    resources.AddIron(380);
    resources.AddOil(310);
    CC_CHECK(FormatResources(resources) == "Iron: 380  Oil: 310");

    // --- selection summary shows type, health, team ---
    Unit unit;
    unit.type = UnitType::HeavyTank;
    ApplyBaseStats(unit);
    unit.health = 320.0f;
    unit.teamID = 1;
    const std::string summary = SelectionSummary(unit);
    CC_CHECK(summary.find("Heavy Tank") != std::string::npos);
    CC_CHECK(summary.find("320/500") != std::string::npos);
    CC_CHECK(summary.find("Team 1") != std::string::npos);

    // --- production tabs partition the production menu order ---
    CC_CHECK(ProductionCategoryOf(UnitType::RifleInfantry) == ProductionCategory::Infantry);
    CC_CHECK(ProductionCategoryOf(UnitType::AntiArmorInfantry) == ProductionCategory::Infantry);
    CC_CHECK(ProductionCategoryOf(UnitType::Engineer) == ProductionCategory::Infantry);
    CC_CHECK(ProductionCategoryOf(UnitType::Medic) == ProductionCategory::Infantry);
    CC_CHECK(ProductionCategoryOf(UnitType::PrototypeInfantry) == ProductionCategory::Infantry);
    CC_CHECK(ProductionCategoryOf(UnitType::IFV) == ProductionCategory::Vehicle);
    CC_CHECK(ProductionCategoryOf(UnitType::Artillery) == ProductionCategory::Vehicle);
    CC_CHECK(ProductionCategoryOf(UnitType::LightTank) == ProductionCategory::Vehicle);
    CC_CHECK(ProductionCategoryOf(UnitType::HeavyTank) == ProductionCategory::Vehicle);
    const std::vector<UnitType> infantry = InfantryMenuOrder();
    const std::vector<UnitType> vehicles = VehicleMenuOrder();
    CC_CHECK(infantry.size() == 4);
    CC_CHECK(vehicles.size() == 4);
    CC_CHECK(infantry[0] == UnitType::RifleInfantry && infantry[3] == UnitType::Medic);
    CC_CHECK(vehicles[0] == UnitType::IFV && vehicles[3] == UnitType::HeavyTank);
    CC_CHECK(infantry.size() + vehicles.size() == ProductionMenuOrder().size());

    // --- building tab covers all placeable types exactly once ---
    const std::vector<BuildingType> buildings = BuildingMenuOrder();
    CC_CHECK(buildings.size() == 4);
    CC_CHECK(buildings[0] == BuildingType::Base);
    CC_CHECK(buildings[1] == BuildingType::ResourceDepot);
    CC_CHECK(buildings[2] == BuildingType::Bootcamp);
    CC_CHECK(buildings[3] == BuildingType::Workshop);

    // --- BuildingHealthFraction: full, half, over-max clamp, max guard ---
    {
        Building full;
        full.health = 400.0f;
        full.maxHealth = 400.0f;
        CC_CHECK(BuildingHealthFraction(full) == 1.0f);
        Building half;
        half.health = 100.0f;
        half.maxHealth = 200.0f;
        CC_CHECK(BuildingHealthFraction(half) == 0.5f);
        Building over;
        over.health = 500.0f;
        over.maxHealth = 400.0f;
        CC_CHECK(BuildingHealthFraction(over) == 1.0f);
        Building zeroMax;
        zeroMax.health = 10.0f;
        zeroMax.maxHealth = 0.0f;
        CC_CHECK(BuildingHealthFraction(zeroMax) == 0.0f);
        Building negativeMax;
        negativeMax.health = 10.0f;
        negativeMax.maxHealth = -5.0f;
        CC_CHECK(BuildingHealthFraction(negativeMax) == 0.0f);
    }

    RunHudAbilityTests();
}

namespace
{

Entity AddHudUnit(Registry &registry, UnitType type)
{
    const Entity id = registry.Create();
    Unit unit;
    unit.type = type;
    registry.Add(id, unit);
    registry.Add(id, Orders{});
    return id;
}

Entity AddHudBuilding(Registry &registry, BuildingType type)
{
    const Entity id = registry.Create();
    Building building;
    building.type = type;
    registry.Add(id, building);
    return id;
}

const AbilityEntry *FindAbility(const std::vector<AbilityEntry> &entries, AbilityId id)
{
    for (const AbilityEntry &entry : entries)
    {
        if (entry.id == id)
        {
            return &entry;
        }
    }
    return nullptr;
}

} // namespace

void RunHudAbilityTests()
{
    // --- empty selection shows nothing ---
    {
        Registry registry;
        CC_CHECK(AbilitiesForSelection(registry, {}).empty());
    }

    // --- single combat unit: stances + orders, Guard active by default ---
    {
        Registry registry;
        const Entity soldier = AddHudUnit(registry, UnitType::RifleInfantry);
        const std::vector<AbilityEntry> entries =
            AbilitiesForSelection(registry, { soldier });
        CC_CHECK(entries.size() == 5);
        CC_CHECK(FindAbility(entries, AbilityId::Hold) != nullptr);
        CC_CHECK(!FindAbility(entries, AbilityId::Hold)->active);
        CC_CHECK(FindAbility(entries, AbilityId::Guard)->active);
        CC_CHECK(FindAbility(entries, AbilityId::AttackMove) != nullptr);
        CC_CHECK(FindAbility(entries, AbilityId::Halt)->enabled);
        CC_CHECK(FindAbility(entries, AbilityId::Repair) == nullptr);
        CC_CHECK(FindAbility(entries, AbilityId::Heal) == nullptr);
    }

    // --- single engineer gains the Repair toggle ---
    {
        Registry registry;
        const Entity engineer = AddHudUnit(registry, UnitType::Engineer);
        std::vector<AbilityEntry> entries = AbilitiesForSelection(registry, { engineer });
        const AbilityEntry *repair = FindAbility(entries, AbilityId::Repair);
        CC_CHECK(repair != nullptr && repair->enabled && !repair->active);
        GetOrders(registry, engineer).autoRepair = true;
        entries = AbilitiesForSelection(registry, { engineer });
        CC_CHECK(FindAbility(entries, AbilityId::Repair)->active);
    }

    // --- single medic gains Heal, mixed foot keeps the intersection ---
    {
        Registry registry;
        const Entity medic = AddHudUnit(registry, UnitType::Medic);
        const Entity soldier = AddHudUnit(registry, UnitType::RifleInfantry);
        const std::vector<AbilityEntry> solo = AbilitiesForSelection(registry, { medic });
        CC_CHECK(FindAbility(solo, AbilityId::Heal) != nullptr);
        const std::vector<AbilityEntry> mixed =
            AbilitiesForSelection(registry, { medic, soldier });
        CC_CHECK(FindAbility(mixed, AbilityId::Heal) != nullptr);
        CC_CHECK(FindAbility(mixed, AbilityId::AttackMove) != nullptr);
        CC_CHECK(FindAbility(mixed, AbilityId::Repair) == nullptr);
    }

    // --- split stances deactivate both toggles ---
    {
        Registry registry;
        const Entity a = AddHudUnit(registry, UnitType::RifleInfantry);
        const Entity b = AddHudUnit(registry, UnitType::RifleInfantry);
        GetOrders(registry, a).stance = Stance::Hold;
        const std::vector<AbilityEntry> entries = AbilitiesForSelection(registry, { a, b });
        CC_CHECK(!FindAbility(entries, AbilityId::Hold)->active);
        CC_CHECK(!FindAbility(entries, AbilityId::Guard)->active);
    }

    // --- either producer selection enables Rally, depot explains the disabled one ---
    {
        Registry registry;
        const Entity bootcamp = AddHudBuilding(registry, BuildingType::Bootcamp);
        const std::vector<AbilityEntry> entries =
            AbilitiesForSelection(registry, { bootcamp });
        CC_CHECK(FindAbility(entries, AbilityId::Rally)->enabled);
        CC_CHECK(FindAbility(entries, AbilityId::Demolish)->enabled);
        const Entity workshop = AddHudBuilding(registry, BuildingType::Workshop);
        const std::vector<AbilityEntry> shopEntries =
            AbilitiesForSelection(registry, { workshop });
        CC_CHECK(FindAbility(shopEntries, AbilityId::Rally)->enabled);
        const Entity depot = AddHudBuilding(registry, BuildingType::ResourceDepot);
        const std::vector<AbilityEntry> depotEntries =
            AbilitiesForSelection(registry, { depot });
        const AbilityEntry *rally = FindAbility(depotEntries, AbilityId::Rally);
        CC_CHECK(!rally->enabled && !rally->reason.empty());
    }

    // --- mixed unit + building selection keeps Halt only ---
    {
        Registry registry;
        const Entity soldier = AddHudUnit(registry, UnitType::RifleInfantry);
        const Entity bootcamp = AddHudBuilding(registry, BuildingType::Bootcamp);
        const std::vector<AbilityEntry> entries =
            AbilitiesForSelection(registry, { soldier, bootcamp });
        CC_CHECK(entries.size() == 1 && entries[0].id == AbilityId::Halt);
    }
}
