
#include "app/data/UnitConfigJson.h"

namespace
{

template <class T>
bool TryLoadKey(cereal::JSONInputArchive &ar, const char *name, T &out)
{
	try
	{
		ar(cereal::make_nvp(name, out));
		return true;
	}
	catch (const cereal::Exception &)
	{
		return false;
	}
}

template <class T>
void TryLoadValue(cereal::JSONInputArchive &ar, const char *camelName, const char *snakeName,
				  T &out)
{
	if (!TryLoadKey(ar, camelName, out) && snakeName != nullptr)
	{
		TryLoadKey(ar, snakeName, out);
	}
}

template <class T>
void TryLoadValue(cereal::JSONInputArchive &ar, const char *name, T &out)
{
	TryLoadValue(ar, name, nullptr, out);
}

template <class T>
void TryLoadOptional(cereal::JSONInputArchive &ar, const char *camelName, const char *snakeName,
					 std::optional<T> &out)
{
	T value{};
	if (TryLoadKey(ar, camelName, value) ||
		(snakeName != nullptr && TryLoadKey(ar, snakeName, value)))
	{
		out = value;
	}
	else
	{
		out = std::nullopt;
	}
}

}

void load(cereal::JSONInputArchive &ar, JsonStats &s)
{
	TryLoadOptional(ar, "health", nullptr, s.health);
	TryLoadValue(ar, "armorType", "armor_type", s.armorType);
	TryLoadValue(ar, "damageType", "damage_type", s.damageType);
	TryLoadOptional(ar, "attackPower", "attack_power", s.attackPower);
	TryLoadOptional(ar, "attackRange", "attack_range", s.attackRange);
	TryLoadOptional(ar, "cooldownTime", "cooldown_time", s.cooldownTime);
	TryLoadOptional(ar, "speed", nullptr, s.speed);
	TryLoadOptional(ar, "sightRange", "sight_range", s.sightRange);
}

void save(cereal::JSONOutputArchive &ar, const JsonStats &s)
{
	ar(cereal::make_nvp("health", s.health.value()),
	   cereal::make_nvp("armorType", s.armorType),
	   cereal::make_nvp("damageType", s.damageType),
	   cereal::make_nvp("attackPower", s.attackPower.value()),
	   cereal::make_nvp("attackRange", s.attackRange.value()),
	   cereal::make_nvp("cooldownTime", s.cooldownTime.value()),
	   cereal::make_nvp("speed", s.speed.value()),
	   cereal::make_nvp("sightRange", s.sightRange.value()));
}

void load(cereal::JSONInputArchive &ar, JsonBounds &b)
{
	TryLoadValue(ar, "left", b.left);
	TryLoadValue(ar, "top", b.top);
	TryLoadValue(ar, "right", b.right);
	TryLoadValue(ar, "bottom", b.bottom);
}

void save(cereal::JSONOutputArchive &ar, const JsonBounds &b)
{
	ar(cereal::make_nvp("left", b.left), cereal::make_nvp("top", b.top),
	   cereal::make_nvp("right", b.right), cereal::make_nvp("bottom", b.bottom));
}

void load(cereal::JSONInputArchive &ar, JsonOrigin &o)
{
	TryLoadValue(ar, "x", o.x);
	TryLoadValue(ar, "y", o.y);
}

void save(cereal::JSONOutputArchive &ar, const JsonOrigin &o)
{
	ar(cereal::make_nvp("x", o.x), cereal::make_nvp("y", o.y));
}

void load(cereal::JSONInputArchive &ar, JsonCollision &c)
{
	TryLoadOptional(ar, "bounds", nullptr, c.bounds);
	TryLoadOptional(ar, "origin", nullptr, c.origin);
}

void save(cereal::JSONOutputArchive &ar, const JsonCollision &c)
{
	ar(cereal::make_nvp("bounds", c.bounds.value()),
	   cereal::make_nvp("origin", c.origin.value()));
}

void load(cereal::JSONInputArchive &ar, JsonArt &a)
{
	TryLoadValue(ar, "spritePrefix", "sprite_prefix", a.spritePrefix);
	TryLoadValue(ar, "atlasIdlePrefix", "atlas_idle_prefix", a.atlasIdlePrefix);
	TryLoadValue(ar, "atlasMovePrefix", "atlas_move_prefix", a.atlasMovePrefix);
}

void save(cereal::JSONOutputArchive &ar, const JsonArt &a)
{
	ar(cereal::make_nvp("spritePrefix", a.spritePrefix),
	   cereal::make_nvp("atlasIdlePrefix", a.atlasIdlePrefix),
	   cereal::make_nvp("atlasMovePrefix", a.atlasMovePrefix));
}

void load(cereal::JSONInputArchive &ar, JsonCost &c)
{
	TryLoadOptional(ar, "iron", nullptr, c.iron);
	TryLoadOptional(ar, "oil", nullptr, c.oil);
}

void save(cereal::JSONOutputArchive &ar, const JsonCost &c)
{
	ar(cereal::make_nvp("iron", c.iron.value()), cereal::make_nvp("oil", c.oil.value()));
}

void load(cereal::JSONInputArchive &ar, JsonTurret &t)
{
	TryLoadOptional(ar, "turnRate", "turn_rate", t.turnRate);
}

void save(cereal::JSONOutputArchive &ar, const JsonTurret &t)
{
	ar(cereal::make_nvp("turnRate", t.turnRate.value()));
}

void load(cereal::JSONInputArchive &ar, JsonArcingShell &s)
{
	TryLoadOptional(ar, "flightTime", "flight_time", s.flightTime);
	TryLoadOptional(ar, "splashTiles", "splash_tiles", s.splashTiles);
}

void save(cereal::JSONOutputArchive &ar, const JsonArcingShell &s)
{
	ar(cereal::make_nvp("flightTime", s.flightTime.value()),
	   cereal::make_nvp("splashTiles", s.splashTiles.value()));
}

void load(cereal::JSONInputArchive &ar, JsonTransport &t)
{
	TryLoadOptional(ar, "capacity", nullptr, t.capacity);
}

void save(cereal::JSONOutputArchive &ar, const JsonTransport &t)
{
	ar(cereal::make_nvp("capacity", t.capacity.value()));
}

void load(cereal::JSONInputArchive &ar, JsonAbilities &a)
{
	TryLoadOptional(ar, "turret", nullptr, a.turret);
	TryLoadOptional(ar, "arcingShell", "arcing_shell", a.arcingShell);
	TryLoadOptional(ar, "crushesFlesh", "crushes_flesh", a.crushesFlesh);
	TryLoadOptional(ar, "transport", nullptr, a.transport);
	TryLoadOptional(ar, "sniper", nullptr, a.sniper);
}

void save(cereal::JSONOutputArchive &ar, const JsonAbilities &a)
{
	ar(cereal::make_nvp("turret", a.turret.value()),
	   cereal::make_nvp("arcingShell", a.arcingShell.value()),
	   cereal::make_nvp("crushesFlesh", a.crushesFlesh.value()),
	   cereal::make_nvp("transport", a.transport.value()),
	   cereal::make_nvp("sniper", a.sniper.value()));
}

void UnitConfigJson::Load(cereal::JSONInputArchive &ar)
{
	TryLoadValue(ar, "type", cfg.type);
	TryLoadValue(ar, "stats", cfg.stats);
	TryLoadValue(ar, "cost", cfg.cost);
	TryLoadValue(ar, "abilities", cfg.abilities);
	TryLoadValue(ar, "collision", cfg.collision);
	TryLoadValue(ar, "art", cfg.art);
}

void UnitConfigJson::Save(cereal::JSONOutputArchive &ar) const
{
	ar(cereal::make_nvp("type", cfg.type), cereal::make_nvp("stats", cfg.stats),
	   cereal::make_nvp("cost", cfg.cost), cereal::make_nvp("abilities", cfg.abilities),
	   cereal::make_nvp("collision", cfg.collision), cereal::make_nvp("art", cfg.art));
}
