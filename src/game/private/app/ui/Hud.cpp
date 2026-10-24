#include "app/ui/Hud.h"

#include <array>
#include <cstdio>

#include "economy/Building.h"
#include "units/Extensions.h"
#include "app/input/Hotkeys.h"
#include "units/UnitStats.h"

const char *UnitTypeName(UnitType type)
{
	switch (type)
	{
	case UnitType::RifleInfantry:
		return "Rifle Infantry";
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
	case UnitType::PrototypeInfantry:
		return "Prototype Infantry";
	case UnitType::Medic:
		return "Medic";
	}
	return "Unknown";
}

const char *BuildingTypeName(BuildingType type)
{
	switch (type)
	{
	case BuildingType::Base:
		return "Base";
	case BuildingType::ResourceDepot:
		return "Depot";
	case BuildingType::Bootcamp:
		return "Bootcamp";
	case BuildingType::Workshop:
		return "Workshop";
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

std::string SelectionLineText(const Registry &registry)
{
	Entity selectedUnit = kInvalidEntity;
	int unitCount = 0;
	registry.Each<Unit>([&](Entity entity, const Unit &unit) {
		if (!unit.isSelected)
		{
			return;
		}
		if (selectedUnit == kInvalidEntity)
		{
			selectedUnit = entity;
		}
		++unitCount;
	});
	if (unitCount > 0)
	{
		const Unit *unit = registry.Get<Unit>(selectedUnit);
		if (unit == nullptr)
		{
			return "No selection";
		}
		if (unitCount > 1)
		{
			char buf[64];
			std::snprintf(buf, sizeof(buf), "%d units selected", unitCount);
			return buf;
		}
		return SelectionSummary(*unit);
	}

	std::array<int, static_cast<size_t>(BuildingType::Count)> counts{};
	int buildingCount = 0;
	int typeCount = 0;
	const Building *only = nullptr;
	registry.Each<Building>([&](Entity, const Building &building) {
		if (!building.isSelected)
		{
			return;
		}
		if (buildingCount == 0)
		{
			only = &building;
		}
		++counts[static_cast<size_t>(building.type)];
		++buildingCount;
	});
	if (buildingCount == 0 || only == nullptr)
	{
		return "No selection";
	}
	if (buildingCount == 1)
	{
		return BuildingSummary(*only);
	}
	for (const int count : counts)
	{
		if (count > 0)
		{
			++typeCount;
		}
	}
	if (typeCount > 2)
	{
		char buf[64];
		std::snprintf(buf, sizeof(buf), "%d buildings selected", buildingCount);
		return buf;
	}
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

const char *StanceName(Stance stance)
{
	switch (stance)
	{
	case Stance::Hold:
		return "Hold";
	case Stance::Guard:
		return "Guard";
	case Stance::Patrol:
		return "Patrol";
	}
	return "Unknown";
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

const char *AbilityName(AbilityId id)
{
	switch (id)
	{
	case AbilityId::Hold:
		return "Hold";
	case AbilityId::Guard:
		return "Guard";
	case AbilityId::Patrol:
		return "Patrol";
	case AbilityId::AttackMove:
		return "AttackMove";
	case AbilityId::Halt:
		return "Halt";
	case AbilityId::Repair:
		return "Repair";
	case AbilityId::Heal:
		return "Heal";
	case AbilityId::Rally:
		return "Rally";
	case AbilityId::Demolish:
		return "Sell";
	}
	return "Unknown";
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
