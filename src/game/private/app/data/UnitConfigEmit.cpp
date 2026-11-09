
#include "app/data/UnitConfigEmit.h"

#include "app/data/UnitConfigJson.h"
#include "app/data/UnitConfigParse.h"

#include <sstream>
#include <string>

std::string UnitConfigToJson(const UnitConfig &config)
{
	UnitConfigJson bridge;
	JsonConfig &out = bridge.cfg;
	out.type = config.type;
	out.stats.health = config.stats.health;
	out.stats.armorType = ArmorTypeName(config.stats.armorType);
	out.stats.damageType = DamageTypeName(config.stats.damageType);
	out.stats.attackPower = config.stats.attackPower;
	out.stats.attackRange = config.stats.attackRangeTiles;
	out.stats.cooldownTime = config.stats.cooldownTime;
	out.stats.speed = config.stats.speedTilesPerSec;
	out.stats.sightRange = config.stats.sightRangeTiles;
	out.cost.iron = config.costIron;
	out.cost.oil = config.costOil;
	{
		JsonTurret turret;
		turret.turnRate = config.abilities.turretTurnRate;
		out.abilities.turret = turret;
		JsonArcingShell shell;
		shell.flightTime = config.abilities.arcingFlightTime;
		shell.splashTiles = config.abilities.arcingSplashTiles;
		out.abilities.arcingShell = shell;
		out.abilities.crushesFlesh = config.abilities.crushesFlesh;
		JsonTransport transport;
		transport.capacity = config.abilities.transportCapacity;
		out.abilities.transport = transport;
		out.abilities.sniper = config.abilities.sniper;
	}
	JsonBounds bounds;
	bounds.left = config.boundLeft;
	bounds.top = config.boundTop;
	bounds.right = config.boundLeft + config.footprintWidth;
	bounds.bottom = config.boundTop + config.footprintHeight;
	out.collision.bounds = bounds;
	JsonOrigin origin;
	origin.x = config.originX;
	origin.y = config.originY;
	out.collision.origin = origin;
	out.art.spritePrefix = config.spritePrefix;
	out.art.atlasIdlePrefix = config.atlasIdlePrefix;
	out.art.atlasMovePrefix = config.atlasMovePrefix;

	std::ostringstream json;
	{
		cereal::JSONOutputArchive ar(json);
		bridge.Save(ar);
	}
	return json.str();
}
