#pragma once

#include <string>
#include <vector>

#include "Registry.h"
#include "ResourceSystem.h"
#include "Unit.h"
#include "Production.h"

enum class BuildingType;
class HotkeyMap;
struct Orders;

// HUD text content: pure functions (tested); the RmlUi HUD owns the panels.

const char *UnitTypeName(UnitType type);
const char *BuildingTypeName(BuildingType type);
std::string FormatResources(const ResourceSystem &resources);
std::string SelectionSummary(const Unit &unit);
float UnitHealthFraction(const Unit &unit);
std::vector<std::string> ShortcutHintLines(const HotkeyMap &hotkeys);
struct HoverTooltipState
{
    Entity hovered = kInvalidEntity;
    float time = 0.0f;
};
constexpr float kHoverTooltipDelay = 0.4f;
bool UpdateHoverTooltip(HoverTooltipState &state, Entity hovered, float dt, float delay);
std::vector<std::string> UnitTooltipLines(const Unit &unit, const Orders &orders);

// Factory production UI. Display order for the 8 unit types; per-tab
// filters partition that order. Building tab enters placement mode.
std::vector<UnitType> ProductionMenuOrder();
enum class ProductionCategory
{
    Infantry,
    Vehicle
};
ProductionCategory ProductionCategoryOf(UnitType type);
std::vector<UnitType> InfantryMenuOrder();
std::vector<UnitType> VehicleMenuOrder();
std::vector<BuildingType> BuildingMenuOrder();
