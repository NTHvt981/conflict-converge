#pragma once

#include <cstdint>
#include <expected>
#include <optional>
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

enum class TileSheet : std::uint8_t
{
	Grass,
	Water,
	Count
};

enum class BuildingFrame
{
	Operational,
	UnderConstruction,
	Count
};

class ArtSprites
{
public:
	// Missing files or Headless load nothing; UseRectangles stays true.
	bool Init(InitMode mode);
	void Shutdown();

	bool IsReady() const;
	bool UseRectangles() const; // draw colored rects instead of sprites

	// True once the atlas JSON parsed AND every sheet texture loaded.
	bool UseAtlas() const;
	// Parse the atlas files without touching the GPU (tests).
	[[nodiscard]] std::expected<void, ConfigError> LoadAtlas();
	// Empty when no sheet or no entries; directional-to-base and static-pose
	// fallbacks. facingDir is a Facing octant (0..7).
	std::string UnitSprite(UnitType type, bool moving, unsigned int id,
						   float timeSeconds, int facingDir, bool attacking = false) const;
	// Seconds; 0 when the atlas is down or the type has no death anim.
	float UnitDeathAnimSeconds(UnitType type) const;
	// Clamped to the anim's duration, holding the last frame past the end.
	// facingDir is a Facing octant (0..7); "" when no death anim exists.
	std::string UnitDeathSprite(UnitType type, unsigned int id, float elapsedSeconds,
								int facingDir) const;

	// Atlas mask tint (mask layer only); color-blind mode uses the Okabe-Ito pair.
	Color TeamTint(TeamId teamID) const;
	void SetColorBlindMode(bool enabled);

	// False on the headless/rectangle path.
	bool HasFont() const;
	const Font &UiFont() const;

	static constexpr int kTileFrames = 8;
	static const char *TileSheetFile(TileSheet sheet);
	// Best render material for a terrain type; none means the 128px art path.
	static std::optional<TileSheet> TileSheetFor(TerrainType type);
	// 0..kTileFrames-1, deterministic per render cell.
	static int TileVariant(int cellX, int cellY);
	// True when every 64px tile sheet loaded and the rectangle fallback is off.
	bool UseTileSheets() const;

	// Foot infantry render 72px cells against the 128px tile; others 1x.
	static float BaseArtScale(UnitType type);
	// `TerrainFile` is the `<stem>_128px.png` stem; `TerrainSlot` indexes
	// terrain_[] in 0..kTerrainSlots-1.
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
	bool colorBlindMode_ = false;
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
	Texture2D tileSheets_[static_cast<int>(TileSheet::Count)] = {}; // [TileSheet]
	Font uiFont_ = {};               // valid only when fontReady_
	bool fontReady_ = false;
};
