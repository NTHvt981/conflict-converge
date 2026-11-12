#pragma once

#include <expected>
#include <string>
#include <unordered_map>

#include "raylib.h"
#include "app/data/InitMode.h"
#include "app/data/SpriteData.h"
#include "core/Team.h"
#include "economy/Building.h"
#include "economy/Nodes.h"
#include "units/Unit.h"

enum class TerrainType : std::uint8_t;

enum class BuildingFrame
{
	Operational,
	UnderConstruction,
	Count
};

// Sprite assets and resolution. Texture arrays for units, buildings, nodes,
// icons, rally markers and terrain tiles, the atlas map + SpriteSheetData, and
// sheet readiness state. Init(InitMode::Headless) is the headless
// rectangle-fallback path; Assets live under data/sprites/ and data/fonts/
// (see Art.h.context.md for the contract).

class ArtSprites
{
public:
	// Load every sprite. Headless or on a missing file, loads
	// nothing and UseRectangles stays true. Returns IsReady.
	bool Init(InitMode mode);
	void Shutdown();

	bool IsReady() const;
	bool UseRectangles() const; // fallback flag: draw colored rects instead

	// True once the atlas JSON parsed AND every sheet texture loaded.
	bool UseAtlas() const;
	// Parse the atlas files without touching the GPU (tests).
	[[nodiscard]] std::expected<void, ConfigError> LoadAtlas();
	// Sprite name for a unit at timeSeconds: directional attack (when
	// attacking and the type has attack anims), walk/idle otherwise, with
	// directional-to-base fallback. Empty when no sheet or no entries.
	std::string UnitSprite(UnitType type, bool moving, unsigned int id,
						   float timeSeconds, int facingDir, bool attacking = false) const;
	// Layered tank art: body follows movement facing, head follows turret aim.
	// facingDir is a Facing octant (0..7); "" when the type has no layered art.
	std::string TankBodySprite(UnitType type, int facingDir) const;
	std::string TankHeadSprite(UnitType type, int facingDir) const;
	// True when the type has layered body art loaded in the atlas.
	bool HasTankArt(UnitType type) const;

	// Team color for atlas mask tinting (mask layer only); color-blind mode
	// swaps in the Okabe-Ito pair.
	Color TeamTint(TeamId teamID) const;
	void SetColorBlindMode(bool enabled);

	// UI typeface; HasFont is false on the headless/rectangle path.
	bool HasFont() const;
	const Font &UiFont() const;

	// Base art scale multiplier (prototype infantry renders at 2x).
	static float BaseArtScale(UnitType type);
	// Terrain filename stem + tile slot per type (`<stem>_128px.png`).
	// Slots index terrain_[], so they run 0..kTerrainSlots-1.
	static constexpr int kTerrainSlots = 4;
	static const char *TerrainFile(TerrainType type);
	static int TerrainSlot(TerrainType type);
	static int TeamSlot(TeamId teamID); // 0 -> blue, anything else -> red

private:
	friend class ArtDraw;

	bool ready_ = false;
	bool fallback_ = true;
	bool atlasReady_ = false;
	bool sheetLoaded_ = false;
	bool colorBlindMode_ = false; // Okabe-Ito team palette when true
	SpriteSheetData sheet_;
	std::unordered_map<std::string, Texture2D> atlas_; // SpriteTextureInfo.id -> sheet
	Texture2D units_[static_cast<int>(UnitType::Count)][2][2] = {}; // [type][team][frame]
	struct BuildingFrameArt
	{
		Texture2D flat[2] = {};
		Texture2D base = {};
		Texture2D mask = {};
	};
	struct BuildingArt
	{
		BuildingFrameArt frames[static_cast<int>(BuildingFrame::Count)];
		bool hasFrame[static_cast<int>(BuildingFrame::Count)] = {};
	};
	BuildingArt buildingArt_[static_cast<int>(BuildingType::Count)];
	Texture2D nodes_[2] = {};        // [kind]
	Texture2D icons_[2] = {};        // [kind]
	Texture2D rallyBase_ = {};
	Texture2D rallyMask_ = {};
	Texture2D terrain_[kTerrainSlots] = {}; // [TerrainSlot]
	Font uiFont_ = {};               // Porto Buena (valid only when fontReady_)
	bool fontReady_ = false;
};
