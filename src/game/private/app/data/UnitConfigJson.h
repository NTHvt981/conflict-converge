#pragma once

#define CEREAL_RAPIDJSON_ASSERT(x) \
	do { if (!(x)) throw std::runtime_error("malformed unit-config JSON"); } while (0)

#include <cereal/archives/json.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include <optional>
#include <stdexcept>
#include <string>

namespace
{

template <class Archive, class T>
bool TryLoadKey(Archive &ar, const char *name, T &out)
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

template <class Archive, class T>
void TryLoadValue(Archive &ar, const char *camelName, const char *snakeName, T &out)
{
	if (!TryLoadKey(ar, camelName, out) && snakeName != nullptr)
	{
		TryLoadKey(ar, snakeName, out);
	}
}

template <class Archive, class T>
void TryLoadValue(Archive &ar, const char *name, T &out)
{
	TryLoadValue(ar, name, nullptr, out);
}

template <class Archive, class T>
void TryLoadOptional(Archive &ar, const char *camelName, const char *snakeName,
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

struct JsonStats
{
	std::optional<float> health;
	std::string armorType;
	std::string damageType;
	std::optional<int> attackPower;
	std::optional<int> attackRange;
	std::optional<float> cooldownTime;
	std::optional<float> speed;
	std::optional<float> sightRange;
	template <class Archive>
	void load(Archive &ar)
	{
		TryLoadOptional(ar, "health", nullptr, health);
		TryLoadValue(ar, "armorType", "armor_type", armorType);
		TryLoadValue(ar, "damageType", "damage_type", damageType);
		TryLoadOptional(ar, "attackPower", "attack_power", attackPower);
		TryLoadOptional(ar, "attackRange", "attack_range", attackRange);
		TryLoadOptional(ar, "cooldownTime", "cooldown_time", cooldownTime);
		TryLoadOptional(ar, "speed", nullptr, speed);
		TryLoadOptional(ar, "sightRange", "sight_range", sightRange);
	}
	template <class Archive>
	void save(Archive &ar) const
	{
		ar(cereal::make_nvp("health", health.value()),
		   cereal::make_nvp("armorType", armorType),
		   cereal::make_nvp("damageType", damageType),
		   cereal::make_nvp("attackPower", attackPower.value()),
		   cereal::make_nvp("attackRange", attackRange.value()),
		   cereal::make_nvp("cooldownTime", cooldownTime.value()),
		   cereal::make_nvp("speed", speed.value()),
		   cereal::make_nvp("sightRange", sightRange.value()));
	}
};

struct JsonBounds
{
	int left = 0;
	int top = 0;
	int right = 0;
	int bottom = 0;
	template <class Archive>
	void load(Archive &ar)
	{
		TryLoadValue(ar, "left", left);
		TryLoadValue(ar, "top", top);
		TryLoadValue(ar, "right", right);
		TryLoadValue(ar, "bottom", bottom);
	}
	template <class Archive>
	void save(Archive &ar) const
	{
		ar(CEREAL_NVP(left), CEREAL_NVP(top), CEREAL_NVP(right), CEREAL_NVP(bottom));
	}
};

struct JsonOrigin
{
	int x = 0;
	int y = 0;
	template <class Archive>
	void load(Archive &ar)
	{
		TryLoadValue(ar, "x", x);
		TryLoadValue(ar, "y", y);
	}
	template <class Archive>
	void save(Archive &ar) const
	{
		ar(CEREAL_NVP(x), CEREAL_NVP(y));
	}
};

struct JsonCollision
{
	std::optional<JsonBounds> bounds;
	std::optional<JsonOrigin> origin;
	template <class Archive>
	void load(Archive &ar)
	{
		TryLoadOptional(ar, "bounds", nullptr, bounds);
		TryLoadOptional(ar, "origin", nullptr, origin);
	}
	template <class Archive>
	void save(Archive &ar) const
	{
		ar(cereal::make_nvp("bounds", bounds.value()),
		   cereal::make_nvp("origin", origin.value()));
	}
};

struct JsonArt
{
	std::string spritePrefix;
	std::string atlasIdlePrefix;
	std::string atlasMovePrefix;
	template <class Archive>
	void load(Archive &ar)
	{
		TryLoadValue(ar, "spritePrefix", "sprite_prefix", spritePrefix);
		TryLoadValue(ar, "atlasIdlePrefix", "atlas_idle_prefix", atlasIdlePrefix);
		TryLoadValue(ar, "atlasMovePrefix", "atlas_move_prefix", atlasMovePrefix);
	}
	template <class Archive>
	void save(Archive &ar) const
	{
		ar(CEREAL_NVP(spritePrefix), CEREAL_NVP(atlasIdlePrefix), CEREAL_NVP(atlasMovePrefix));
	}
};

struct JsonCost
{
	std::optional<int> iron;
	std::optional<int> oil;
	template <class Archive>
	void load(Archive &ar)
	{
		TryLoadOptional(ar, "iron", nullptr, iron);
		TryLoadOptional(ar, "oil", nullptr, oil);
	}
	template <class Archive>
	void save(Archive &ar) const
	{
		ar(cereal::make_nvp("iron", iron.value()), cereal::make_nvp("oil", oil.value()));
	}
};

struct JsonTurret
{
	std::optional<float> turnRate;
	template <class Archive>
	void load(Archive &ar)
	{
		TryLoadOptional(ar, "turnRate", "turn_rate", turnRate);
	}
	template <class Archive>
	void save(Archive &ar) const
	{
		ar(cereal::make_nvp("turnRate", turnRate.value()));
	}
};

struct JsonArcingShell
{
	std::optional<float> flightTime;
	std::optional<int> splashTiles;
	template <class Archive>
	void load(Archive &ar)
	{
		TryLoadOptional(ar, "flightTime", "flight_time", flightTime);
		TryLoadOptional(ar, "splashTiles", "splash_tiles", splashTiles);
	}
	template <class Archive>
	void save(Archive &ar) const
	{
		ar(cereal::make_nvp("flightTime", flightTime.value()),
		   cereal::make_nvp("splashTiles", splashTiles.value()));
	}
};

struct JsonTransport
{
	std::optional<int> capacity;
	template <class Archive>
	void load(Archive &ar)
	{
		TryLoadOptional(ar, "capacity", nullptr, capacity);
	}
	template <class Archive>
	void save(Archive &ar) const
	{
		ar(cereal::make_nvp("capacity", capacity.value()));
	}
};

struct JsonAbilities
{
	std::optional<JsonTurret> turret;
	std::optional<JsonArcingShell> arcingShell;
	std::optional<bool> crushesFlesh;
	std::optional<JsonTransport> transport;
	std::optional<bool> sniper;
	template <class Archive>
	void load(Archive &ar)
	{
		TryLoadOptional(ar, "turret", nullptr, turret);
		TryLoadOptional(ar, "arcingShell", "arcing_shell", arcingShell);
		TryLoadOptional(ar, "crushesFlesh", "crushes_flesh", crushesFlesh);
		TryLoadOptional(ar, "transport", nullptr, transport);
		TryLoadOptional(ar, "sniper", nullptr, sniper);
	}
	template <class Archive>
	void save(Archive &ar) const
	{
		ar(cereal::make_nvp("turret", turret.value()),
		   cereal::make_nvp("arcingShell", arcingShell.value()),
		   cereal::make_nvp("crushesFlesh", crushesFlesh.value()),
		   cereal::make_nvp("transport", transport.value()),
		   cereal::make_nvp("sniper", sniper.value()));
	}
};

struct JsonConfig
{
	std::string type;
	JsonStats stats;
	JsonCost cost;
	JsonAbilities abilities;
	JsonCollision collision;
	JsonArt art;
};

}
