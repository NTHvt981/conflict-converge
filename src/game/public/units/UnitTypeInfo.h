#pragma once

#include <cstddef>

#include "units/Locomotor.h"
#include "units/Unit.h"

struct UnitTypeInfo
{
	const char *displayName;
	const char *configName;      // JSON "type" value
	const char *fileStem;        // data/configs/<fileStem>.json
	const char *spriteStem;      // spritePrefix default + data/sprites stem
	const char *artDir;          // data/sprites/units/<artDir>/
	const char *tankArtPrefix;   // nullptr = no body/head atlas
	const char *atlasIdlePrefix; // "" = flat-PNG fallback
	const char *atlasMovePrefix; // "" = flat-PNG fallback
	bool isVehicle;
	Locomotor locomotor;
	int ironCost;
	int oilCost;
	cc::IVec2 footprint;
};

inline constexpr UnitTypeInfo kUnitTypes[] = {
	{ "Rifle Infantry", "RifleInfantry", "rifle_infantry", "rifle_infantry", "rifle",
	  nullptr, "rifle_infantry_idle", "rifle_infantry_move", false, Locomotor::Foot, 25, 0,
	  { 1, 1 } },
	{ "Anti-Armor", "AntiArmorInfantry", "antiarmor_infantry", "antiarmor", "antiarmor",
	  nullptr, "antiarmor_idle", "antiarmor_move", false, Locomotor::Foot, 30, 5, { 1, 1 } },
	{ "Engineer", "Engineer", "engineer", "engineer", "engineer",
	  nullptr, "engineer_idle", "engineer_move", false, Locomotor::Foot, 40, 0, { 1, 1 } },
	{ "IFV", "IFV", "ifv", "ifv", "ifv",
	  nullptr, "", "", true, Locomotor::Wheeled, 80, 20, { 2, 2 } },
	{ "Artillery", "Artillery", "artillery", "artillery", "artillery",
	  nullptr, "", "", true, Locomotor::Wheeled, 120, 40, { 2, 2 } },
	{ "Light Tank", "LightTank", "light_tank", "lighttank", "lighttank",
	  "lighttank", "", "", true, Locomotor::Tracked, 150, 50, { 1, 1 } },
	{ "Heavy Tank", "HeavyTank", "heavy_tank", "heavytank", "heavytank",
	  "heavytank", "", "", true, Locomotor::Tracked, 250, 100, { 2, 2 } },
	{ "Prototype Infantry", "PrototypeInfantry", "prototype_infantry", "prototypeinfantry",
	  "prototypeinfantry", nullptr, "", "", false, Locomotor::Foot, 25, 0, { 1, 1 } },
	{ "Medic", "Medic", "medic", "medic", "medic",
	  nullptr, "medic_idle", "medic_move", false, Locomotor::Foot, 30, 0, { 1, 1 } },
};

static_assert(sizeof(kUnitTypes) / sizeof(kUnitTypes[0]) ==
				  static_cast<std::size_t>(UnitType::Count),
			  "one row per UnitType");

inline const UnitTypeInfo &InfoOf(UnitType type)
{
	return kUnitTypes[static_cast<std::size_t>(type)];
}
