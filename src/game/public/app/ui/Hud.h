#pragma once

#include <string>
#include <vector>

#include "core/Registry.h"
#include "economy/ResourceSystem.h"
#include "units/Unit.h"
#include "units/UnitFactory.h"
#include "units/UnitStats.h"
#include "economy/Production.h"

enum class BuildingType;
struct Building;
class HotkeyMap;
struct Orders;

// Pure text builders; the RmlUi HUD owns the panels.

const char *UnitTypeName(UnitType type);
const char *BuildingTypeName(BuildingType type);
std::string FormatResources(const ResourceSystem &resources);
std::string ProductionRowText(const char *name, UnitCost cost);
bool CanAfford(const ResourceSystem &resources, UnitCost cost);
std::string QueueLineText(int bootcampQueue, int workshopQueue);
std::string SelectionSummary(const Unit &unit);
float UnitHealthFraction(const Unit &unit);
float BuildingHealthFraction(const Building &building);
std::string BuildingSummary(const Building &building);
std::string SelectionLineText(const Registry &registry);
std::vector<std::string> ShortcutHintLines(const HotkeyMap &hotkeys);
struct HoverTooltipState
{
	Entity hovered = kInvalidEntity;
	float time = 0.0f;
};
constexpr float kHoverTooltipDelay = 0.4f;
bool UpdateHoverTooltip(HoverTooltipState &state, Entity hovered, float dt, float delay);
std::vector<std::string> UnitTooltipLines(const Unit &unit, const Orders &orders);

// Display order; per-tab filters partition it. Building tab enters placement mode.
std::vector<UnitType> ProductionMenuOrder();
std::vector<UnitType> InfantryMenuOrder();
std::vector<UnitType> VehicleMenuOrder();
std::vector<BuildingType> BuildingMenuOrder();

// Pure selection-context query; the RmlUi HUD renders it, commands execute it.
// active = every selected unit has it; reason is non-empty only when disabled.
enum class AbilityId
{
	Hold,
	Guard,
	Patrol,
	AttackMove,
	Halt,
	Repair,
	Heal,
	Rally,
	Demolish,
	Count
};
const char *AbilityName(AbilityId id);
struct AbilityEntry
{
	AbilityId id = AbilityId::Halt;
	bool enabled = true;
	bool active = false;
	std::string reason;
};
std::vector<AbilityEntry> AbilitiesForSelection(const Registry &registry,
												 const std::vector<Entity> &selection);
