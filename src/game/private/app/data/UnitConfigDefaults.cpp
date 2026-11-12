
#include "app/data/UnitConfigDefaults.h"

#include "app/data/UnitConfig.h"
#include "units/UnitTypeInfo.h"

UnitConfig DefaultUnitConfig(UnitType type)
{
	const UnitTypeInfo &info = InfoOf(type);
	UnitConfig config;
	config.type = info.configName;
	config.stats = BaseStats(type);
	config.costIron = info.ironCost;
	config.costOil = info.oilCost;
	config.abilities = UnitAbilities{};
	if (type == UnitType::HeavyTank)
	{
		config.abilities.crushesFlesh = true;
	}
	// G1: turreted tanks aim body-independent heads (visible traverse).
	if (type == UnitType::LightTank || type == UnitType::HeavyTank)
	{
		config.abilities.turretTurnRate = 3.0f;
	}
	// G2: artillery lobs dodgeable shells (visible arc, small AoE). Tuned
	// so Medium closes soak games: 0.6s flight, splash 1 (foot cannot
	// outwalk ±half tile + hitbox, vehicles still dodge).
	if (type == UnitType::Artillery)
	{
		config.abilities.arcingFlightTime = 0.6f;
		config.abilities.arcingSplashTiles = 1;
	}
	// G4: IFV carries foot infantry (player gestures only; the AI never
	// builds carriers or boards — transport AI is a separate project).
	if (type == UnitType::IFV)
	{
		config.abilities.transportCapacity = 4;
	}
	// G5: AntiArmorInfantry is the designated marksman — 25 power is ~25%
	// of a 100HP squad, so every hit visibly removes one soldier through
	// the existing SquadSlots thresholds. True per-sprite HP deferred.
	if (type == UnitType::AntiArmorInfantry)
	{
		config.abilities.sniper = true;
	}
	config.footprintWidth = info.isVehicle ? 2 : 1;
	config.footprintHeight = info.isVehicle ? 2 : 1;
	config.spritePrefix = info.spriteStem;
	config.atlasIdlePrefix = info.atlasIdlePrefix;
	config.atlasMovePrefix = info.atlasMovePrefix;
	return config;
}
