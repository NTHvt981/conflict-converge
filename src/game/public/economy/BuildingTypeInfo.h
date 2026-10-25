#pragma once

#include <cstddef>

#include "core/MathUtils.h"
#include "economy/Building.h"

struct BuildingTypeInfo
{
	const char *displayName;
	const char *flatStem;   // nullptr = no flat PNG
	const char *maskedStem; // nullptr = no masked PNG
	cc::IVec2 footprint;
	float maxHealth;
	float buildTime;
	long ironCost;
	long oilCost;
	bool isProducer;
	ProductionCategory producerCategory;
};

inline constexpr BuildingTypeInfo kBuildingTypes[] = {
	{ "Base", "base", nullptr, { 2, 2 }, 400.0f, 3.0f, 350, 100, false,
	  ProductionCategory::Infantry },
	{ "Depot", "depot", nullptr, { 1, 1 }, 200.0f, 2.0f, 150, 0, false,
	  ProductionCategory::Infantry },
	{ "Bootcamp", nullptr, "bootcamp", { 2, 2 }, 350.0f, 4.0f, 200, 0, true,
	  ProductionCategory::Infantry },
	{ "Workshop", nullptr, "workshop", { 3, 3 }, 500.0f, 5.0f, 300, 0, true,
	  ProductionCategory::Vehicle },
};

static_assert(sizeof(kBuildingTypes) / sizeof(kBuildingTypes[0]) ==
				  static_cast<std::size_t>(BuildingType::Count),
			  "one row per BuildingType");

inline const BuildingTypeInfo &InfoOf(BuildingType type)
{
	return kBuildingTypes[static_cast<std::size_t>(type)];
}
