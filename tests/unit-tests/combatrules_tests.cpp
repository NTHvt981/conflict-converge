// Unit tests for the combat-rules config layer (damage-matrix parse
// validation, rejection cases, the runtime Effectiveness hookup, and the
// directory refresh path). Renderer-adjacent gameplay stays untested per
// project convention.

#include "test_harness.h"

#include "app/data/CombatRules.h"
#include "app/data/ConfigError.h"
#include "units/Combat.h"
#include "units/UnitStats.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{

std::string TempDir(const char *name)
{
	return (std::filesystem::temp_directory_path() / name).string();
}

void WriteFile(const std::string &path, const char *contents)
{
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	out << contents;
}

const char *kConfigDirs[] = { "data/configs", "../../data/configs",
							  "../../../data/configs" };

} // namespace

void RunCombatRulesTests()
{
	ResetCombatRules();

	// --- defaults: the compiled matrix is the historical table ---
	{
		const CombatRules defaults = DefaultCombatRules();
		const float expected[3][3] = { { 1.00f, 0.75f, 0.50f },
									   { 1.25f, 1.00f, 0.75f },
									   { 0.50f, 1.25f, 1.00f } };
		for (int row = 0; row < 3; ++row)
		{
			for (int col = 0; col < 3; ++col)
			{
				CC_CHECK(defaults.damageMatrix[row][col] == expected[row][col]);
			}
		}
	}

	// --- shipped seed file parses to the defaults, Effectiveness follows ---
	{
		bool found = false;
		for (const char *dir : kConfigDirs)
		{
			const std::string path = std::string(dir) + "/combat_rules.json";
			const std::expected<CombatRules, ConfigError> loaded =
				LoadCombatRulesFile(path);
			if (!loaded)
			{
				continue;
			}
			const CombatRules file = *loaded;
			const CombatRules defaults = DefaultCombatRules();
			for (int row = 0; row < 3; ++row)
			{
				for (int col = 0; col < 3; ++col)
				{
					CC_CHECK(file.damageMatrix[row][col] ==
							 defaults.damageMatrix[row][col]);
				}
			}
			found = true;
			break;
		}
		CC_CHECK(found);
		CC_CHECK(Effectiveness(DamageType::KINETIC, ArmorType::STEEL) == 1.00f);
		CC_CHECK(Effectiveness(DamageType::EXPLOSIVE, ArmorType::STEEL) == 1.25f);
		CC_CHECK(Effectiveness(DamageType::ENERGY, ArmorType::COMPOSITE) == 1.00f);
	}

	// --- runtime hook: override feeds Effectiveness, reset restores it ---
	{
		CombatRules custom = DefaultCombatRules();
		custom.damageMatrix[static_cast<int>(DamageType::KINETIC)]
						   [static_cast<int>(ArmorType::STEEL)] = 2.0f;
		SetActiveCombatRules(custom);
		CC_CHECK(Effectiveness(DamageType::KINETIC, ArmorType::STEEL) == 2.0f);
		ResetCombatRules();
		CC_CHECK(Effectiveness(DamageType::KINETIC, ArmorType::STEEL) == 1.00f);
	}

	// --- rejections: malformed JSON -> MalformedJson ---
	{
		const std::expected<CombatRules, ConfigError> garbage =
			ParseCombatRulesJson("{ this is not json");
		CC_CHECK(!garbage.has_value() && garbage.error() == ConfigError::MalformedJson);
		const std::expected<CombatRules, ConfigError> wrongType =
			ParseCombatRulesJson("{\"damageMatrix\":5}");
		CC_CHECK(!wrongType.has_value() &&
				 wrongType.error() == ConfigError::MalformedJson);
	}

	// --- rejections: unknown enum names -> UnknownName ---
	{
		const std::expected<CombatRules, ConfigError> badDamage = ParseCombatRulesJson(
			"{\"damageMatrix\":{\"PLASMA\":{\"RUBBER\":1.0,\"STEEL\":2.0,"
			"\"COMPOSITE\":3.0}}}");
		CC_CHECK(!badDamage.has_value() &&
				 badDamage.error() == ConfigError::UnknownName);
		const std::expected<CombatRules, ConfigError> badArmor = ParseCombatRulesJson(
			"{\"damageMatrix\":{\"KINETIC\":{\"WOOD\":1.0,\"STEEL\":2.0,"
			"\"COMPOSITE\":3.0}}}");
		CC_CHECK(!badArmor.has_value() &&
				 badArmor.error() == ConfigError::UnknownName);
	}

	// --- rejections: missing matrix key or cell -> InvalidData ---
	{
		const std::expected<CombatRules, ConfigError> missing = ParseCombatRulesJson("{}");
		CC_CHECK(!missing.has_value() && missing.error() == ConfigError::InvalidData);
		const std::expected<CombatRules, ConfigError> holes = ParseCombatRulesJson(
			"{\"damageMatrix\":{\"KINETIC\":{\"RUBBER\":1.0,\"STEEL\":2.0},"
			"\"EXPLOSIVE\":{\"RUBBER\":1.25,\"STEEL\":1.0,\"COMPOSITE\":0.75},"
			"\"ENERGY\":{\"RUBBER\":0.5,\"STEEL\":1.25,\"COMPOSITE\":1.0}}}");
		CC_CHECK(!holes.has_value() && holes.error() == ConfigError::InvalidData);
	}

	// --- rejections: zero or negative cells -> InvalidData ---
	{
		const std::expected<CombatRules, ConfigError> zero = ParseCombatRulesJson(
			"{\"damageMatrix\":{\"KINETIC\":{\"RUBBER\":0.0,\"STEEL\":2.0,"
			"\"COMPOSITE\":3.0},\"EXPLOSIVE\":{\"RUBBER\":1.25,\"STEEL\":1.0,"
			"\"COMPOSITE\":0.75},\"ENERGY\":{\"RUBBER\":0.5,\"STEEL\":1.25,"
			"\"COMPOSITE\":1.0}}}");
		CC_CHECK(!zero.has_value() && zero.error() == ConfigError::InvalidData);
		const std::expected<CombatRules, ConfigError> negative = ParseCombatRulesJson(
			"{\"damageMatrix\":{\"KINETIC\":{\"RUBBER\":-1.0,\"STEEL\":2.0,"
			"\"COMPOSITE\":3.0},\"EXPLOSIVE\":{\"RUBBER\":1.25,\"STEEL\":1.0,"
			"\"COMPOSITE\":0.75},\"ENERGY\":{\"RUBBER\":0.5,\"STEEL\":1.25,"
			"\"COMPOSITE\":1.0}}}");
		CC_CHECK(!negative.has_value() && negative.error() == ConfigError::InvalidData);
	}

	// --- refresh: a temp dir with a modified file installs it ---
	{
		const std::string dir = TempDir("cc_combatrules_refresh");
		std::error_code ec;
		std::filesystem::create_directories(dir, ec);
		WriteFile(dir + "/combat_rules.json",
				  "{\"damageMatrix\":{\"KINETIC\":{\"STEEL\":2.0,\"RUBBER\":0.5,"
				  "\"COMPOSITE\":0.25},\"EXPLOSIVE\":{\"STEEL\":1.25,\"RUBBER\":1.0,"
				  "\"COMPOSITE\":0.75},\"ENERGY\":{\"STEEL\":0.5,\"RUBBER\":1.25,"
				  "\"COMPOSITE\":1.0}}}");
		CC_CHECK(RefreshCombatRules(dir));
		CC_CHECK(Effectiveness(DamageType::KINETIC, ArmorType::STEEL) == 2.0f);
		CC_CHECK(Effectiveness(DamageType::KINETIC, ArmorType::COMPOSITE) == 0.25f);

		// Missing file: silent false, active rules keep the last good values.
		CC_CHECK(!RefreshCombatRules(TempDir("cc_combatrules_empty")));
		CC_CHECK(Effectiveness(DamageType::KINETIC, ArmorType::STEEL) == 2.0f);
		std::filesystem::remove_all(dir, ec);
		ResetCombatRules();
	}

	// --- refresh: a rejected file never touches the active rules ---
	{
		const std::string dir = TempDir("cc_combatrules_reject");
		std::error_code ec;
		std::filesystem::create_directories(dir, ec);
		WriteFile(dir + "/combat_rules.json", "{ garbage");
		CC_CHECK(!RefreshCombatRules(dir));
		const CombatRules defaults = DefaultCombatRules();
		const CombatRules &active = ActiveCombatRules();
		for (int row = 0; row < 3; ++row)
		{
			for (int col = 0; col < 3; ++col)
			{
				CC_CHECK(active.damageMatrix[row][col] == defaults.damageMatrix[row][col]);
			}
		}
		std::filesystem::remove_all(dir, ec);
		ResetCombatRules();
	}
}
