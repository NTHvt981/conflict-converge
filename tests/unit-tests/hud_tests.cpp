// Unit tests for HUD text builders (the raygui Draw* wrappers need
// a window and are exercised live in main.cpp, not here).

#include "test_harness.h"

#include "Hud.h"
#include "UnitStats.h"

void RunHudTests()
{
    // --- unit type names cover all 8 types ---
    CC_CHECK(std::string(UnitTypeName(UnitType::Infantry)) == "Infantry");
    CC_CHECK(std::string(UnitTypeName(UnitType::AntiArmorInfantry)) == "Anti-Armor");
    CC_CHECK(std::string(UnitTypeName(UnitType::Engineer)) == "Engineer");
    CC_CHECK(std::string(UnitTypeName(UnitType::IFV)) == "IFV");
    CC_CHECK(std::string(UnitTypeName(UnitType::Artillery)) == "Artillery");
    CC_CHECK(std::string(UnitTypeName(UnitType::LightTank)) == "Light Tank");
    CC_CHECK(std::string(UnitTypeName(UnitType::HeavyTank)) == "Heavy Tank");
    CC_CHECK(std::string(UnitTypeName(UnitType::PrototypeInfantry)) == "Prototype Infantry");

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
}
