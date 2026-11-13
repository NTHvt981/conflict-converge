#include "app/data/ArtSprites.h"

#include <cstdio>

#include "economy/BuildingTypeInfo.h"
#include "units/UnitTypeInfo.h"
#include "world/TileMap.h"

namespace
{

const char *BuildingFrameSuffix(BuildingFrame frame)
{
	switch (frame)
	{
	case BuildingFrame::UnderConstruction:
		return "construction";
	case BuildingFrame::Operational:
	case BuildingFrame::Count:
		return "";
	}
	return "";
}

const char *NodeFile(ResourceKind kind)
{
	return kind == ResourceKind::Iron ? "iron" : "oil";
}

const char *TeamName(TeamId teamID)
{
	return teamID == kPlayerTeam ? "blue" : "red";
}

int AnimTotalMs(const SpriteAnimInfo &anim)
{
	int total = 0;
	for (const SpriteAnimFrame &f : anim.frames)
	{
		total += f.durationMs;
	}
	return total;
}

// Sprite name for the anim frame at tMs. Wrapping playback reduces tMs
// modulo the total; clamped playback holds the last frame past the end.
std::string AnimFrameName(const SpriteSheetData &sheet, const SpriteAnimInfo &anim,
						  int tMs, bool clamped)
{
	const int total = AnimTotalMs(anim);
	if (total <= 0)
	{
		return {};
	}
	int t = tMs;
	if (clamped)
	{
		t = t < 0 ? 0 : (t > total - 1 ? total - 1 : t);
	}
	else
	{
		t %= total;
		if (t < 0)
		{
			t += total;
		}
	}
	for (const SpriteAnimFrame &f : anim.frames)
	{
		if (t < f.durationMs)
		{
			if (const SpriteDefInfo *s = FindSpriteById(sheet, f.sprite); s != nullptr)
			{
				return s->name;
			}
			break;
		}
		t -= f.durationMs;
	}
	return {};
}

struct TerrainArtInfo
{
	const char *file; // nullptr = no tile art
	int slot;         // index into terrain_
};

constexpr TerrainArtInfo kTerrainArt[] = {
	{ "grass", 0 },
	{ "water", 1 },
	{ nullptr, -1 },
	{ "forest", 2 },
	{ "rock", 3 },
};

static_assert(sizeof(kTerrainArt) / sizeof(kTerrainArt[0]) ==
				  static_cast<std::size_t>(TerrainType::Count),
			  "one row per TerrainType");

constexpr bool TerrainSlotsFit()
{
	for (const TerrainArtInfo &art : kTerrainArt)
	{
		if (art.slot >= ArtSprites::kTerrainSlots)
		{
			return false;
		}
	}
	return true;
}

static_assert(TerrainSlotsFit(), "terrain slot exceeds ArtSprites::kTerrainSlots");

}

const char *ArtSprites::TerrainFile(TerrainType type)
{
	return kTerrainArt[static_cast<std::size_t>(type)].file;
}

int ArtSprites::TerrainSlot(TerrainType type)
{
	return kTerrainArt[static_cast<std::size_t>(type)].slot;
}

int ArtSprites::TeamSlot(TeamId teamID)
{
	return teamID == kPlayerTeam ? 0 : 1;
}

bool ArtSprites::Init(InitMode mode)
{
	Shutdown();
	if (mode == InitMode::Headless)
	{
		return false;
	}
	fallback_ = false;
	char path[128];
	bool unitFlatMissing[static_cast<int>(UnitType::Count)] = {};
	for (int t = 0; t < static_cast<int>(UnitType::Count); ++t)
	{
		for (int team = 0; team < 2; ++team)
		{
			for (int f = 0; f < 2; ++f)
			{
				std::snprintf(path, sizeof(path), "data/sprites/units/%s/%s_%s_%s.png",
							  InfoOf(static_cast<UnitType>(t)).artDir,
							  InfoOf(static_cast<UnitType>(t)).spriteStem,
							  team == 0 ? "blue" : "red", f == 0 ? "idle" : "attack");
				units_[t][team][f] = LoadTexture(path);
				if (units_[t][team][f].id == 0)
				{
					unitFlatMissing[t] = true;
				}
			}
		}
	}
	for (int t = 0; t < static_cast<int>(BuildingType::Count); ++t)
	{
		const BuildingType type = static_cast<BuildingType>(t);
		if (const char *flat = InfoOf(type).flatStem; flat != nullptr)
		{
			for (int f = 0; f < static_cast<int>(BuildingFrame::Count); ++f)
			{
				const BuildingFrame frame = static_cast<BuildingFrame>(f);
				const char *suffix = BuildingFrameSuffix(frame);
				Texture2D loaded[2] = {};
				bool complete = true;
				for (int team = 0; team < 2; ++team)
				{
					if (f == 0)
					{
						std::snprintf(path, sizeof(path), "data/sprites/buildings/%s_%s.png",
									  flat, team == 0 ? "blue" : "red");
					}
					else
					{
						std::snprintf(path, sizeof(path),
									  "data/sprites/buildings/%s_%s_%s.png", flat, suffix,
									  team == 0 ? "blue" : "red");
					}
					loaded[team] = LoadTexture(path);
					if (loaded[team].id == 0)
					{
						complete = false;
					}
				}
				if (f == 0)
				{
					buildingArt_[t].frames[0].flat[0] = loaded[0];
					buildingArt_[t].frames[0].flat[1] = loaded[1];
					buildingArt_[t].hasFrame[0] = true;
					if (!complete)
					{
						fallback_ = true;
					}
				}
				else if (complete)
				{
					buildingArt_[t].frames[f].flat[0] = loaded[0];
					buildingArt_[t].frames[f].flat[1] = loaded[1];
					buildingArt_[t].hasFrame[f] = true;
				}
				else
				{
					for (int team = 0; team < 2; ++team)
					{
						if (loaded[team].id != 0)
						{
							UnloadTexture(loaded[team]);
						}
					}
				}
			}
		}
		else if (const char *masked = InfoOf(type).maskedStem; masked != nullptr)
		{
			for (int f = 0; f < static_cast<int>(BuildingFrame::Count); ++f)
			{
				const BuildingFrame frame = static_cast<BuildingFrame>(f);
				const char *suffix = BuildingFrameSuffix(frame);
				Texture2D base = {};
				Texture2D mask = {};
				if (f == 0)
				{
					std::snprintf(path, sizeof(path), "data/sprites/buildings/%s_base.png",
								  masked);
					base = LoadTexture(path);
					std::snprintf(path, sizeof(path), "data/sprites/buildings/%s_mask.png",
								  masked);
					mask = LoadTexture(path);
				}
				else
				{
					std::snprintf(path, sizeof(path),
								  "data/sprites/buildings/%s_%s_base.png", masked, suffix);
					base = LoadTexture(path);
					std::snprintf(path, sizeof(path),
								  "data/sprites/buildings/%s_%s_mask.png", masked, suffix);
					mask = LoadTexture(path);
				}
				if (base.id != 0 && mask.id != 0)
				{
					buildingArt_[t].frames[f].base = base;
					buildingArt_[t].frames[f].mask = mask;
					buildingArt_[t].hasFrame[f] = true;
				}
				else
				{
					if (base.id != 0)
					{
						UnloadTexture(base);
					}
					if (mask.id != 0)
					{
						UnloadTexture(mask);
					}
					if (f == 0)
					{
						fallback_ = true;
					}
				}
			}
		}
		else
		{
			fallback_ = true;
		}
	}
	for (int k = 0; k < 2; ++k)
	{
		std::snprintf(path, sizeof(path), "data/sprites/nodes/%s.png",
					  NodeFile(static_cast<ResourceKind>(k)));
		nodes_[k] = LoadTexture(path);
		std::snprintf(path, sizeof(path), "data/sprites/icons/%s.png",
					  NodeFile(static_cast<ResourceKind>(k)));
		icons_[k] = LoadTexture(path);
		if (nodes_[k].id == 0 || icons_[k].id == 0)
		{
			fallback_ = true;
		}
	}
	rallyBase_ = LoadTexture("data/sprites/icons/rally_point_base.png");
	rallyMask_ = LoadTexture("data/sprites/icons/rally_point_mask.png");
	for (const TerrainArtInfo &art : kTerrainArt)
	{
		if (art.slot < 0)
		{
			continue;
		}
		std::snprintf(path, sizeof(path), "data/sprites/terrain_tiles/%s_128px.png", art.file);
		terrain_[art.slot] = LoadTexture(path);
		if (terrain_[art.slot].id == 0)
		{
			fallback_ = true;
		}
	}
	uiFont_ = LoadFontEx("data/fonts/font.otf", 64, nullptr, 0);
	fontReady_ = uiFont_.texture.id != 0;
	if (!fontReady_)
	{
		fallback_ = true;
	}
	const std::expected<void, ConfigError> atlas = LoadAtlas();
	atlasReady_ = atlas.has_value();
	if (!atlas)
	{
		TraceLog(LOG_WARNING, "sprite atlas rejected: %s", Describe(atlas.error()));
	}
	if (atlasReady_)
	{
		for (const SpriteTextureInfo &t : sheet_.textures)
		{
			std::snprintf(path, sizeof(path), "data/sprites/%s", t.file.c_str());
			const Texture2D tex = LoadTexture(path);
			if (tex.id == 0)
			{
				atlasReady_ = false;
				break;
			}
			atlas_[t.id] = tex;
		}
		if (!atlasReady_)
		{
			for (const auto &entry : atlas_)
			{
				UnloadTexture(entry.second);
			}
			atlas_.clear();
			sheet_ = SpriteSheetData{};
			sheetLoaded_ = false;
		}
	}
	for (int t = 0; t < static_cast<int>(UnitType::Count); ++t)
	{
		if (!unitFlatMissing[t])
		{
			continue;
		}
		const std::string idleName =
			std::string(InfoOf(static_cast<UnitType>(t)).spriteStem) + "_idle_0_6";
		if (!(atlasReady_ && FindSpriteByName(sheet_, idleName) != nullptr))
		{
			fallback_ = true;
		}
	}
	ready_ = !fallback_;
	return ready_;
}

void ArtSprites::Shutdown()
{
	for (int t = 0; t < static_cast<int>(UnitType::Count); ++t)
	{
		for (int team = 0; team < 2; ++team)
		{
			for (int f = 0; f < 2; ++f)
			{
				if (units_[t][team][f].id != 0)
				{
					UnloadTexture(units_[t][team][f]);
					units_[t][team][f] = {};
				}
			}
		}
	}
	for (int t = 0; t < static_cast<int>(BuildingType::Count); ++t)
	{
		for (int f = 0; f < static_cast<int>(BuildingFrame::Count); ++f)
		{
			BuildingFrameArt &frame = buildingArt_[t].frames[f];
			for (int team = 0; team < 2; ++team)
			{
				if (frame.flat[team].id != 0)
				{
					UnloadTexture(frame.flat[team]);
					frame.flat[team] = {};
				}
			}
			if (frame.base.id != 0)
			{
				UnloadTexture(frame.base);
				frame.base = {};
			}
			if (frame.mask.id != 0)
			{
				UnloadTexture(frame.mask);
				frame.mask = {};
			}
			buildingArt_[t].hasFrame[f] = false;
		}
	}
	for (int k = 0; k < 2; ++k)
	{
		if (nodes_[k].id != 0)
		{
			UnloadTexture(nodes_[k]);
			nodes_[k] = {};
		}
		if (icons_[k].id != 0)
		{
			UnloadTexture(icons_[k]);
			icons_[k] = {};
		}
	}
	if (rallyBase_.id != 0)
	{
		UnloadTexture(rallyBase_);
		rallyBase_ = {};
	}
	if (rallyMask_.id != 0)
	{
		UnloadTexture(rallyMask_);
		rallyMask_ = {};
	}
	for (int s = 0; s < ArtSprites::kTerrainSlots; ++s)
	{
		if (terrain_[s].id != 0)
		{
			UnloadTexture(terrain_[s]);
			terrain_[s] = {};
		}
	}
	if (fontReady_)
	{
		UnloadFont(uiFont_);
		uiFont_ = {};
		fontReady_ = false;
	}
	for (const auto &entry : atlas_)
	{
		UnloadTexture(entry.second);
	}
	atlas_.clear();
	sheet_ = SpriteSheetData{};
	atlasReady_ = false;
	sheetLoaded_ = false;
	ready_ = false;
	fallback_ = true;
}

bool ArtSprites::IsReady() const
{
	return ready_;
}

bool ArtSprites::UseRectangles() const
{
	return fallback_;
}

bool ArtSprites::UseAtlas() const
{
	return atlasReady_;
}

std::expected<void, ConfigError> ArtSprites::LoadAtlas()
{
	const std::expected<SpriteSheetData, ConfigError> sheet = LoadSpriteSheet();
	if (!sheet)
	{
		return std::unexpected(sheet.error());
	}
	sheet_ = *sheet;
	sheetLoaded_ = true;
	return {};
}

bool ArtSprites::HasFont() const
{
	return fontReady_;
}

const Font &ArtSprites::UiFont() const
{
	return uiFont_;
}

std::string ArtSprites::UnitSprite(UnitType type, bool moving, unsigned int id,
								   float timeSeconds, int facingDir, bool attacking) const
{
	if (!sheetLoaded_)
	{
		return {};
	}
	const int dir = (facingDir < 0 || facingDir > 7) ? 6 : facingDir;
	const std::string prefix = InfoOf(type).spriteStem;
	auto animSprite = [&](const std::string &animName) -> std::string {
		const SpriteAnimInfo *anim = FindAnimByName(sheet_, animName);
		if (anim == nullptr || anim->frames.empty())
		{
			return {};
		}
		const int total = AnimTotalMs(*anim);
		if (total <= 0)
		{
			return {};
		}
		const int t = static_cast<int>(timeSeconds * 1000.0f) +
					  static_cast<int>(id * 37u % static_cast<unsigned int>(total));
		return AnimFrameName(sheet_, *anim, t, false);
	};
	if (attacking)
	{
		const std::string names[] = { prefix + "_attack_" + std::to_string(dir),
									  prefix + "_attack" };
		for (const std::string &name : names)
		{
			if (std::string sprite = animSprite(name); !sprite.empty())
			{
				return sprite;
			}
		}
	}
	if (moving)
	{
		const std::string names[] = { prefix + "_move_" + std::to_string(dir),
									  prefix + "_move" };
		for (const std::string &name : names)
		{
			if (std::string sprite = animSprite(name); !sprite.empty())
			{
				return sprite;
			}
		}
	}
	{
		const std::string names[] = { prefix + "_idle_" + std::to_string(dir),
									  prefix + "_idle" };
		for (const std::string &name : names)
		{
			if (std::string sprite = animSprite(name); !sprite.empty())
			{
				return sprite;
			}
		}
	}
	if (const SpriteDefInfo *idle =
			FindSpriteByName(sheet_, prefix + "_idle_0_" + std::to_string(dir));
		idle != nullptr)
	{
		return idle->name;
	}
	if (const SpriteDefInfo *baseIdle =
			FindSpriteByName(sheet_, prefix + "_idle_0_0");
		baseIdle != nullptr)
	{
		return baseIdle->name;
	}
	return {};
}

bool ArtSprites::HasUnitDeathAnim(UnitType type) const
{
	if (!sheetLoaded_)
	{
		return false;
	}
	return FindAnimByName(sheet_,
						  std::string(InfoOf(type).spriteStem) + "_death_0") != nullptr;
}

std::string ArtSprites::UnitDeathSprite(UnitType type, unsigned int id,
										float elapsedSeconds, int facingDir) const
{
	if (!sheetLoaded_)
	{
		return {};
	}
	(void)id;
	const int dir = (facingDir < 0 || facingDir > 7) ? 6 : facingDir;
	const std::string prefix = InfoOf(type).spriteStem;
	const std::string names[] = { prefix + "_death_" + std::to_string(dir),
								  prefix + "_death" };
	for (const std::string &name : names)
	{
		const SpriteAnimInfo *anim = FindAnimByName(sheet_, name);
		if (anim == nullptr || anim->frames.empty())
		{
			continue;
		}
		const int t = static_cast<int>(elapsedSeconds * 1000.0f);
		if (std::string sprite = AnimFrameName(sheet_, *anim, t, true); !sprite.empty())
		{
			return sprite;
		}
	}
	return {};
}

bool ArtSprites::HasTankArt(UnitType type) const
{
	const char *prefix = InfoOf(type).tankArtPrefix;
	if (prefix == nullptr || !sheetLoaded_)
	{
		return false;
	}
	return FindSpriteByName(sheet_, std::string(prefix) + "_body_idle_0_0") != nullptr;
}

std::string ArtSprites::TankBodySprite(UnitType type, int facingDir) const
{
	const char *prefix = InfoOf(type).tankArtPrefix;
	if (prefix == nullptr || !sheetLoaded_)
	{
		return {};
	}
	const int dir = (facingDir < 0 || facingDir > 7) ? 6 : facingDir;
	const std::string name =
		std::string(prefix) + "_body_idle_0_" + std::to_string(dir);
	return FindSpriteByName(sheet_, name) != nullptr ? name : std::string{};
}

std::string ArtSprites::TankHeadSprite(UnitType type, int facingDir) const
{
	const char *prefix = InfoOf(type).tankArtPrefix;
	if (prefix == nullptr || !sheetLoaded_)
	{
		return {};
	}
	const int dir = (facingDir < 0 || facingDir > 7) ? 6 : facingDir;
	const std::string name = std::string(prefix) + "_head_0_" + std::to_string(dir);
	return FindSpriteByName(sheet_, name) != nullptr ? name : std::string{};
}

Color ArtSprites::TeamTint(TeamId teamID) const
{
	if (colorBlindMode_)
	{
		return teamID == kPlayerTeam ? Color{ 230, 159, 0, 255 } : Color{ 86, 180, 233, 255 };
	}
	return teamID == kPlayerTeam ? BLUE : RED;
}

void ArtSprites::SetColorBlindMode(bool enabled)
{
	colorBlindMode_ = enabled;
}

float ArtSprites::BaseArtScale(UnitType type)
{
	switch (type)
	{
	case UnitType::RifleInfantry:
	case UnitType::AntiArmorInfantry:
	case UnitType::Engineer:
	case UnitType::Medic:
		return 0.8f;
	default:
		return 1.0f;
	}
}
