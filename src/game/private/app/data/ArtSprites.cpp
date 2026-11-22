#include "app/data/ArtSprites.h"

#include <cstdio>
#include <utility>

#include "core/MathUtils.h"
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
	{ nullptr, -1 },
	{ nullptr, -1 },
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

constexpr int kPlayerTextureSlot = 0;
constexpr int kEnemyTextureSlot = 1;

constexpr int kMaxBlendMaterials = 2;

struct BlendEdgeGroup
{
	TileSheet material = TileSheet::Count;
	int sideMask = 0;
};

int SideBit(int side)
{
	return 1 << side;
}

int FindBlendGroup(const BlendEdgeGroup *groups, int count, TileSheet material)
{
	for (int i = 0; i < count; ++i)
	{
		if (groups[i].material == material)
		{
			return i;
		}
	}
	return -1;
}

int CollectBlendGroups(TileSheet base, const TileSheet *neighbours, BlendEdgeGroup *groups)
{
	const int baseRank = ArtSprites::TileMaterialRank(base);
	if (baseRank < 0)
	{
		return 0;
	}
	int count = 0;
	for (int side = 0; side < ArtSprites::kTileSides; ++side)
	{
		const TileSheet material = neighbours[side];
		if (material == TileSheet::Count ||
			ArtSprites::TileMaterialRank(material) <= baseRank)
		{
			continue;
		}
		int slot = FindBlendGroup(groups, count, material);
		if (slot < 0)
		{
			if (count >= kMaxBlendMaterials)
			{
				continue;
			}
			slot = count;
			groups[slot] = { material, 0 };
			++count;
		}
		groups[slot].sideMask |= SideBit(side);
	}
	return count;
}

void OrderBlendGroupsByRank(BlendEdgeGroup *groups, int count)
{
	if (count < kMaxBlendMaterials)
	{
		return;
	}
	if (ArtSprites::TileMaterialRank(groups[1].material) <
		ArtSprites::TileMaterialRank(groups[0].material))
	{
		std::swap(groups[0], groups[1]);
	}
}

int EmitBlendDraws(TileSheet base, const BlendEdgeGroup *groups, int count,
				   ArtSprites::TileBlendDraw *out)
{
	int written = 0;
	for (int i = 0; i < count; ++i)
	{
		const int piece = ArtSprites::BlendPieceIndex(groups[i].sideMask);
		const std::optional<BlendSheet> sheet =
			ArtSprites::BlendSheetFor(base, groups[i].material);
		if (piece < 0 || !sheet.has_value())
		{
			continue;
		}
		out[written] = { *sheet, piece };
		++written;
	}
	return written;
}

}

const char *ArtSprites::TerrainFile(TerrainType type)
{
	return kTerrainArt[static_cast<std::size_t>(type)].file;
}

int ArtSprites::TerrainSlot(TerrainType type)
{
	return kTerrainArt[static_cast<std::size_t>(type)].slot;
}

const char *ArtSprites::TileSheetFile(TileSheet sheet)
{
	switch (sheet)
	{
	case TileSheet::Grass:
		return "grass";
	case TileSheet::Water:
		return "water";
	case TileSheet::Dirt:
		return "dirt";
	case TileSheet::Road:
		return "road";
	case TileSheet::Count:
		break;
	}
	return nullptr;
}

std::optional<TileSheet> ArtSprites::TileSheetFor(TerrainType type)
{
	switch (type)
	{
	case TerrainType::Grass:
		return TileSheet::Grass;
	case TerrainType::Water:
		return TileSheet::Water;
	case TerrainType::Dirt:
		return TileSheet::Dirt;
	case TerrainType::Road:
		return TileSheet::Road;
	default:
		break;
	}
	return std::nullopt;
}

int ArtSprites::WaterFrame(float seconds)
{
	if (seconds <= 0.0f)
	{
		return 0;
	}
	const int step = static_cast<int>(seconds / kWaterFrameSeconds);
	return step % kTileFrames;
}

int ArtSprites::TileVariant(int cellX, int cellY)
{
	constexpr unsigned int kXSalt = 0x9E3779B9u;
	constexpr unsigned int kYSalt = 0x85EBCA6Bu;
	constexpr unsigned int kAvalancheMultiplier = 0x45D9F3Bu;
	constexpr int kAvalancheShift = 16;

	unsigned int hash = static_cast<unsigned int>(cellX) * kXSalt ^
						static_cast<unsigned int>(cellY) * kYSalt;
	hash ^= hash >> kAvalancheShift;
	hash *= kAvalancheMultiplier;
	hash ^= hash >> kAvalancheShift;
	return static_cast<int>(hash % static_cast<unsigned int>(kTileFrames));
}

bool ArtSprites::UseTileSheets() const
{
	if (UseRectangles())
	{
		return false;
	}
	for (const Texture2D &tex : tileSheets_)
	{
		if (tex.id == 0)
		{
			return false;
		}
	}
	return true;
}

bool ArtSprites::UseTileBlends() const
{
	if (UseRectangles())
	{
		return false;
	}
	for (const Texture2D &tex : blendSheets_)
	{
		if (tex.id == 0)
		{
			return false;
		}
	}
	return true;
}

const char *ArtSprites::BlendSheetFile(BlendSheet sheet)
{
	switch (sheet)
	{
	case BlendSheet::GrassDirt:
		return "blend_grass_dirt";
	case BlendSheet::GrassRoad:
		return "blend_grass_road";
	case BlendSheet::DirtRoad:
		return "blend_dirt_road";
	case BlendSheet::Count:
		break;
	}
	return nullptr;
}

std::optional<BlendSheet> ArtSprites::BlendSheetFor(TileSheet base, TileSheet edge)
{
	if (base == TileSheet::Grass && edge == TileSheet::Dirt)
	{
		return BlendSheet::GrassDirt;
	}
	if (base == TileSheet::Grass && edge == TileSheet::Road)
	{
		return BlendSheet::GrassRoad;
	}
	if (base == TileSheet::Dirt && edge == TileSheet::Road)
	{
		return BlendSheet::DirtRoad;
	}
	return std::nullopt;
}

int ArtSprites::TileMaterialRank(TileSheet sheet)
{
	switch (sheet)
	{
	case TileSheet::Grass:
		return 0;
	case TileSheet::Dirt:
		return 1;
	case TileSheet::Road:
		return 2;
	default:
		break;
	}
	return -1;
}

int ArtSprites::BlendPieceIndex(int sideMask)
{
	switch (sideMask)
	{
	case 1:
		return 0;
	case 2:
		return 1;
	case 4:
		return 2;
	case 8:
		return 3;
	case 3:
		return 4;
	case 6:
		return 5;
	case 12:
		return 6;
	case 9:
		return 7;
	default:
		break;
	}
	return -1;
}

int ArtSprites::PlanTileBlends(TileSheet base,
							   const TileSheet neighbours[kTileSides],
							   TileBlendDraw out[kTileSides])
{
	BlendEdgeGroup groups[kMaxBlendMaterials];
	const int count = CollectBlendGroups(base, neighbours, groups);
	OrderBlendGroupsByRank(groups, count);
	return EmitBlendDraws(base, groups, count, out);
}

int ArtSprites::TeamSlot(TeamId teamID)
{
	return teamID == kPlayerTeam ? kPlayerTextureSlot : kEnemyTextureSlot;
}

bool ArtSprites::Init(InitMode mode)
{
	Shutdown();
	if (mode == InitMode::Headless)
	{
		return false;
	}
	fallback_ = false;
	bool unitFlatMissing[static_cast<int>(UnitType::Count)] = {};
	LoadUnitSheets(unitFlatMissing);
	LoadBuildingSheets();
	LoadNodeSheets();
	LoadOverlaySheets();
	LoadFallbackTerrainSheets();
	LoadTileSheets();
	LoadBlendSheets();
	LoadUiFont();
	LoadAtlasSheets();
	CheckUnitArtCoverage(unitFlatMissing);
	ready_ = !fallback_;
	return ready_;
}

void ArtSprites::LoadUnitSheets(bool *flatMissing)
{
	char path[128];
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
					flatMissing[t] = true;
				}
			}
		}
	}
}

void ArtSprites::LoadBuildingSheets()
{
	char path[128];
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
}

void ArtSprites::LoadNodeSheets()
{
	char path[128];
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
}

void ArtSprites::LoadOverlaySheets()
{
	rallyBase_ = LoadTexture("data/sprites/icons/rally_point_base.png");
	rallyMask_ = LoadTexture("data/sprites/icons/rally_point_mask.png");
}

void ArtSprites::LoadFallbackTerrainSheets()
{
	char path[128];
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
}

void ArtSprites::LoadTileSheets()
{
	char path[128];
	for (int s = 0; s < static_cast<int>(TileSheet::Count); ++s)
	{
		const char *file = TileSheetFile(static_cast<TileSheet>(s));
		if (file == nullptr)
		{
			continue;
		}
		std::snprintf(path, sizeof(path), "data/sprites/tiles/%s.png", file);
		tileSheets_[s] = LoadTexture(path);
		if (tileSheets_[s].id == 0 ||
			tileSheets_[s].width != kTileFrames * static_cast<int>(cc::kRenderTileSize))
		{
			if (tileSheets_[s].id != 0)
			{
				UnloadTexture(tileSheets_[s]);
				tileSheets_[s] = {};
			}
			fallback_ = true;
		}
	}
}

void ArtSprites::LoadBlendSheets()
{
	char path[128];
	for (int s = 0; s < static_cast<int>(BlendSheet::Count); ++s)
	{
		const char *file = BlendSheetFile(static_cast<BlendSheet>(s));
		if (file == nullptr)
		{
			continue;
		}
		std::snprintf(path, sizeof(path), "data/sprites/tiles/%s.png", file);
		blendSheets_[s] = LoadTexture(path);
		if (blendSheets_[s].id == 0 ||
			blendSheets_[s].width != kBlendPieces * static_cast<int>(cc::kRenderTileSize))
		{
			if (blendSheets_[s].id != 0)
			{
				UnloadTexture(blendSheets_[s]);
				blendSheets_[s] = {};
			}
			fallback_ = true;
		}
	}
}

void ArtSprites::LoadUiFont()
{
	uiFont_ = LoadFontEx("data/fonts/font.otf", 64, nullptr, 0);
	fontReady_ = uiFont_.texture.id != 0;
	if (!fontReady_)
	{
		fallback_ = true;
	}
}

void ArtSprites::LoadAtlasSheets()
{
	char path[128];
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
}

void ArtSprites::CheckUnitArtCoverage(const bool *flatMissing)
{
	for (int t = 0; t < static_cast<int>(UnitType::Count); ++t)
	{
		if (!flatMissing[t])
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
	for (int s = 0; s < static_cast<int>(TileSheet::Count); ++s)
	{
		if (tileSheets_[s].id != 0)
		{
			UnloadTexture(tileSheets_[s]);
			tileSheets_[s] = {};
		}
	}
	for (int s = 0; s < static_cast<int>(BlendSheet::Count); ++s)
	{
		if (blendSheets_[s].id != 0)
		{
			UnloadTexture(blendSheets_[s]);
			blendSheets_[s] = {};
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

float ArtSprites::UnitDeathAnimSeconds(UnitType type) const
{
	if (!sheetLoaded_)
	{
		return 0.0f;
	}
	if (const SpriteAnimInfo *anim =
			FindAnimByName(sheet_, std::string(InfoOf(type).spriteStem) + "_death_0");
		anim != nullptr)
	{
		return static_cast<float>(AnimTotalMs(*anim)) / 1000.0f;
	}
	return 0.0f;
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
	case UnitType::LaserInfantry:
		return 0.8f;
	default:
		return 1.0f;
	}
}
