#include "app/ui/Hud.h"

#include <array>
#include <cstdio>

#include "economy/Building.h"
#include "economy/BuildingTypeInfo.h"
#include "units/Extensions.h"
#include "units/UnitTypeInfo.h"
#include "app/input/Hotkeys.h"
#include "units/UnitStats.h"

const char *UnitTypeName(UnitType type)
{
	return InfoOf(type).displayName;
}

const char *BuildingTypeName(BuildingType type)
{
	return InfoOf(type).displayName;
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

float BuildingHealthFraction(const Building &building)
{
	if (building.maxHealth <= 0.0f)
	{
		return 0.0f;
	}
	const float fraction = building.health / building.maxHealth;
	if (fraction <= 0.0f)
	{
		return 0.0f;
	}
	return fraction >= 1.0f ? 1.0f : fraction;
}

std::string BuildingSummary(const Building &building)
{
	char buf[128];
	std::snprintf(buf, sizeof(buf), "%s  HP %.0f/%.0f", BuildingTypeName(building.type), building.health,
				  building.maxHealth);
	return buf;
}

namespace
{
constexpr const char *kNoSelectionLine = "No selection";
using BuildingTypeCounts = std::array<int, static_cast<size_t>(BuildingType::Count)>;

std::string UnitsSelectedLine(int count)
{
	char buf[64];
	std::snprintf(buf, sizeof(buf), "%d units selected", count);
	return buf;
}

std::string BuildingsSelectedLine(int count)
{
	char buf[64];
	std::snprintf(buf, sizeof(buf), "%d buildings selected", count);
	return buf;
}

std::string BuildingTypesLine(const BuildingTypeCounts &counts)
{
	std::string line;
	for (size_t i = 0; i < counts.size(); ++i)
	{
		if (counts[i] == 0)
		{
			continue;
		}
		if (!line.empty())
		{
			line += ", ";
		}
		char buf[64];
		std::snprintf(buf, sizeof(buf), "%d %s", counts[i],
					  BuildingTypeName(static_cast<BuildingType>(i)));
		line += buf;
	}
	return line;
}
} // namespace

std::string SelectionLineText(const Registry &registry)
{
	const Unit *selectedUnit = nullptr;
	int unitCount = 0;
	registry.Each<Unit>([&](Entity, const Unit &unit) {
		if (!unit.isSelected)
		{
			return;
		}
		if (selectedUnit == nullptr)
		{
			selectedUnit = &unit;
		}
		++unitCount;
	});
	if (unitCount == 1)
	{
		return SelectionSummary(*selectedUnit);
	}
	if (unitCount > 1)
	{
		return UnitsSelectedLine(unitCount);
	}

	const Building *selectedBuilding = nullptr;
	BuildingTypeCounts buildingsByType{};
	int buildingCount = 0;
	int buildingTypeCount = 0;
	registry.Each<Building>([&](Entity, const Building &building) {
		if (!building.isSelected)
		{
			return;
		}
		if (selectedBuilding == nullptr)
		{
			selectedBuilding = &building;
		}
		int &count = buildingsByType[static_cast<size_t>(building.type)];
		if (count == 0)
		{
			++buildingTypeCount;
		}
		++count;
		++buildingCount;
	});
	if (buildingCount == 0)
	{
		return kNoSelectionLine;
	}
	if (buildingCount == 1)
	{
		return BuildingSummary(*selectedBuilding);
	}
	if (buildingTypeCount > 2)
	{
		return BuildingsSelectedLine(buildingCount);
	}
	return BuildingTypesLine(buildingsByType);
}

namespace
{
std::string HintKey(const HotkeyMap &hotkeys, const char *action, bool applyChord = true)
{
	const HotkeyDef *def = HotkeyDefFor(action);
	const int key = hotkeys.KeyFor(action != nullptr ? action : "");
	std::string name = key <= 0 ? "-" : HotkeyDisplayName(key);
	if (applyChord && def != nullptr && def->chord)
	{
		name = "Shift+" + name;
	}
	return name;
}
}

std::vector<std::string> ShortcutHintLines(const HotkeyMap &hotkeys)
{
	std::vector<std::string> lines = {
		"WASD Camera",
		"Left Select",
		"Right Order",
		"Right-click foot: Load, self: Unload",
	};
	lines.push_back(HintKey(hotkeys, "Back") + " Deselect");
	lines.push_back(HintKey(hotkeys, "Halt") + " Halt");
	lines.push_back(HintKey(hotkeys, "TogglePause") + " Pause");
	lines.push_back(HintKey(hotkeys, "ToggleHints") + " Hints");
	lines.push_back(HintKey(hotkeys, "Quicksave") + " Save");
	lines.push_back(HintKey(hotkeys, "Quickload") + " Load");
	lines.push_back(HintKey(hotkeys, "AttackMove") + " AttackMove");
	lines.push_back(HintKey(hotkeys, "StanceHold") + " Hold");
	lines.push_back(HintKey(hotkeys, "StanceGuard") + " Guard");
	lines.push_back(HintKey(hotkeys, "Patrol") + " Patrol");
	lines.push_back(HintKey(hotkeys, "Rally") + " Rally");
	lines.push_back(HintKey(hotkeys, "SelectType") + " SelectType");
	lines.push_back(HintKey(hotkeys, "SelectFactories") + " Production");
	lines.push_back(HintKey(hotkeys, "SlowestSpeed") + " SlowMove");
	lines.push_back(HintKey(hotkeys, "AreaBuild") + " Place");
	lines.push_back("Alt+RDrag Line");
	lines.push_back(HintKey(hotkeys, "AttackGround") + " AttackGnd");
	lines.push_back(HintKey(hotkeys, "AutoRetreat") + " AutoRetreat");
	lines.push_back(HintKey(hotkeys, "JumpPing") + " JumpPing");
	lines.push_back("Shift Queues");
	lines.push_back("Wheel Zoom");
	lines.push_back(HintKey(hotkeys, "SaveSlot1") + "/" + HintKey(hotkeys, "SaveSlot2") + "/" +
					HintKey(hotkeys, "SaveSlot3") + " SaveSlot");
	lines.push_back("Shift+" + HintKey(hotkeys, "LoadSlot1", false) + "/" +
					HintKey(hotkeys, "LoadSlot2", false) + "/" +
					HintKey(hotkeys, "LoadSlot3", false) + " Load");
	lines.push_back("Ctrl+1-0 Group");
	lines.push_back("Shift+1-0 AddGrp");
	lines.push_back("1-0 Recall");
	lines.push_back("C+S+1-0 AutoAdd");
	return lines;
}

bool UpdateHoverTooltip(HoverTooltipState &state, Entity hovered, float dt, float delay)
{
	if (hovered != state.hovered)
	{
		state.hovered = hovered;
		state.time = 0.0f;
		return false;
	}
	state.time += dt;
	return state.hovered != kInvalidEntity && state.time >= delay;
}

namespace
{

constexpr const char *kStanceNames[] = {
	"Hold",
	"Guard",
	"Patrol",
};
static_assert(sizeof(kStanceNames) / sizeof(kStanceNames[0]) ==
				  static_cast<std::size_t>(Stance::Count),
			  "one row per Stance");

const char *StanceName(Stance stance)
{
	const std::size_t index = static_cast<std::size_t>(stance);
	if (index >= sizeof(kStanceNames) / sizeof(kStanceNames[0]))
	{
		return "Unknown";
	}
	return kStanceNames[index];
}

const char *UnitStateName(UnitState state)
{
	switch (state)
	{
	case UnitState::Idle:
		return "Idle";
	case UnitState::Moving:
		return "Moving";
	case UnitState::Attacking:
		return "Attacking";
	case UnitState::Count:
		return "Unknown";
	default:
		return "Unknown";
	}
}

}

std::vector<std::string> UnitTooltipLines(const Unit &unit, const Orders &orders)
{
	char second[64];
	std::snprintf(second, sizeof(second), "%s, %s", StanceName(orders.stance),
				  UnitStateName(unit.state));
	return { SelectionSummary(unit), second };
}

std::vector<UnitType> ProductionMenuOrder()
{
	return { UnitType::RifleInfantry,          UnitType::AntiArmorInfantry, UnitType::Engineer,
			 UnitType::Medic,                 UnitType::IFV,               UnitType::Artillery,
			 UnitType::LightTank,             UnitType::HeavyTank };
}

std::vector<UnitType> InfantryMenuOrder()
{
	std::vector<UnitType> out;
	for (UnitType type : ProductionMenuOrder())
	{
		if (ProductionCategoryOf(type) == ProductionCategory::Infantry)
		{
			out.push_back(type);
		}
	}
	return out;
}

std::vector<UnitType> VehicleMenuOrder()
{
	std::vector<UnitType> out;
	for (UnitType type : ProductionMenuOrder())
	{
		if (ProductionCategoryOf(type) == ProductionCategory::Vehicle)
		{
			out.push_back(type);
		}
	}
	return out;
}

std::vector<BuildingType> BuildingMenuOrder()
{
	return { BuildingType::Base, BuildingType::ResourceDepot, BuildingType::Bootcamp,
			 BuildingType::Workshop };
}

constexpr const char *kAbilityNames[] = {
	"Hold", "Guard", "Patrol", "AttackMove", "Halt", "Repair", "Heal", "Rally", "Sell",
};
static_assert(sizeof(kAbilityNames) / sizeof(kAbilityNames[0]) ==
				  static_cast<std::size_t>(AbilityId::Count),
			  "one row per AbilityId");

const char *AbilityName(AbilityId id)
{
	const std::size_t index = static_cast<std::size_t>(id);
	if (index >= sizeof(kAbilityNames) / sizeof(kAbilityNames[0]))
	{
		return "Unknown";
	}
	return kAbilityNames[index];
}

std::vector<AbilityEntry> AbilitiesForSelection(const Registry &registry,
												 const std::vector<Entity> &selection)
{
	std::vector<Entity> units;
	std::vector<Entity> buildings;
	for (Entity entity : selection)
	{
		if (registry.Get<Unit>(entity) != nullptr)
		{
			units.push_back(entity);
		}
		else if (registry.Get<Building>(entity) != nullptr)
		{
			buildings.push_back(entity);
		}
	}
	if (!units.empty() && !buildings.empty())
	{
		return { AbilityEntry{ AbilityId::Halt } };
	}
	if (!units.empty())
	{
		bool allHold = true;
		bool allGuard = true;
		bool allPatrol = true;
		bool allAttackMove = true;
		bool anyEngineer = false;
		bool anyMedic = false;
		bool allEngineerRepair = true;
		bool allMedicHeal = true;
		for (Entity entity : units)
		{
			const Unit *unit = registry.Get<Unit>(entity);
			const Orders *orders = registry.Get<Orders>(entity);
			const Stance stance = orders != nullptr ? orders->stance : Stance::Guard;
			allHold = allHold && stance == Stance::Hold;
			allGuard = allGuard && stance == Stance::Guard;
			allPatrol = allPatrol && orders != nullptr && orders->hasPatrol;
			allAttackMove = allAttackMove && orders != nullptr && orders->attackMove;
			const bool ordered = orders != nullptr && orders->hasRepairOrder;
			if (unit->type == UnitType::Engineer)
			{
				anyEngineer = true;
				allEngineerRepair = allEngineerRepair && orders != nullptr && orders->autoRepair;
			}
			if (unit->type == UnitType::Medic)
			{
				anyMedic = true;
				allMedicHeal = allMedicHeal && ordered;
			}
		}
		std::vector<AbilityEntry> out = {
			AbilityEntry{ AbilityId::Hold, true, allHold, {} },
			AbilityEntry{ AbilityId::Guard, true, allGuard, {} },
			AbilityEntry{ AbilityId::Patrol, true, allPatrol, {} },
			AbilityEntry{ AbilityId::AttackMove, true, allAttackMove, {} },
			AbilityEntry{ AbilityId::Halt },
		};
		if (anyEngineer)
		{
			out.push_back(AbilityEntry{ AbilityId::Repair, true, allEngineerRepair, {} });
		}
		if (anyMedic)
		{
			out.push_back(AbilityEntry{ AbilityId::Heal, true, allMedicHeal, {} });
		}
		return out;
	}
	if (!buildings.empty())
	{
		bool anyProducer = false;
		for (Entity entity : buildings)
		{
			if (const Building *building = registry.Get<Building>(entity);
				building != nullptr && ProducerCategory(building->type).has_value())
			{
				anyProducer = true;
			}
		}
		return {
			AbilityEntry{ AbilityId::Rally, anyProducer, false,
						  anyProducer ? std::string{} : std::string{ "Requires Bootcamp / Workshop" } },
			AbilityEntry{ AbilityId::Demolish },
		};
	}
	return {};
}
