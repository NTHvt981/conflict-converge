// Unit tests for HUD text builders (the raygui Draw* wrappers need
// a window and are exercised live in main.cpp, not here).

#include "test_harness.h"

#include "Building.h"
#include "Hud.h"
#include "UnitStats.h"

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

    // --- production tabs partition the factory menu order ---
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
    CC_CHECK(buildings.size() == 3);
    CC_CHECK(buildings[0] == BuildingType::Base);
    CC_CHECK(buildings[1] == BuildingType::ResourceDepot);
    CC_CHECK(buildings[2] == BuildingType::Factory);
}
