#include "Hud.h"

#include <cstdio>

#include "raygui.h"   // panels/labels
#include "Selection.h" // SelectedUnit
#include "UnitStats.h" // max-health lookup for the summary

const char *UnitTypeName(UnitType type)
{
    switch (type)
    {
    case UnitType::Infantry:
        return "Infantry";
    case UnitType::AntiArmorInfantry:
        return "Anti-Armor";
    case UnitType::Engineer:
        return "Engineer";
    case UnitType::IFV:
        return "IFV";
    case UnitType::Artillery:
        return "Artillery";
    case UnitType::LightTank:
        return "Light Tank";
    case UnitType::HeavyTank:
        return "Heavy Tank";
    }
    return "Unknown";
}

std::string FormatResources(const ResourceSystem &resources)
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "Iron: %ld  Oil: %ld", resources.iron, resources.oil);
    return buf;
}

std::string SelectionSummary(const Unit &unit)
{
    const float maxHealth = BaseStats(unit.type).health;
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%s  HP %.0f/%.0f  Team %d", UnitTypeName(unit.type), unit.health,
                  maxHealth, unit.teamID);
    return buf;
}

float UnitHealthFraction(const Unit &unit)
{
    const float maxHealth = BaseStats(unit.type).health;
    if (maxHealth <= 0.0f)
    {
        return 0.0f;
    }
    const float fraction = unit.health / maxHealth;
    if (fraction <= 0.0f)
    {
        return 0.0f;
    }
    return fraction >= 1.0f ? 1.0f : fraction;
}

std::vector<std::string> ShortcutHintLines()
{
    return {
        "WASD Camera",
        "Left Select",
        "Right Order",
        "Esc Deselect",
        "Space Halt",
        "P Pause",
        "F1 Hints",
    };
}

void DrawResourcePanel(const ResourceSystem &resources)
{
    GuiPanel({ 8.0f, 8.0f, 220.0f, 56.0f }, "Stockpile");
    GuiLabel({ 20.0f, 32.0f, 200.0f, 20.0f }, FormatResources(resources).c_str());
}

void DrawSelectionPanel(Registry &registry)
{
    const Entity selected = SelectedUnit(registry);
    const Unit *unit = registry.Get<Unit>(selected);
    GuiPanel({ 250.0f, 386.0f, 300.0f, 56.0f }, "Selection");
    const char *text = "No selection";
    std::string summary;
    if (unit != nullptr)
    {
        summary = SelectionSummary(*unit);
        text = summary.c_str();
    }
    GuiLabel({ 262.0f, 410.0f, 280.0f, 20.0f }, text);
}
