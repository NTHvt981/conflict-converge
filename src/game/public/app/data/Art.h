#pragma once

#include <optional>
#include <string>

#include "raylib.h"
#include "app/data/ArtDraw.h"
#include "app/data/ArtSprites.h"
#include "app/data/InitMode.h"
#include "core/Subsystem.h"
#include "core/Team.h"

enum class TerrainType : std::uint8_t;

class Art : public Subsystem
{
public:
	// Missing files or Headless load nothing; UseRectangles stays true.
	bool Init(InitMode mode);
	void Shutdown();

	bool IsReady() const;
	bool UseRectangles() const; // draw colored rects instead of sprites

	// tileCorner = unit.position (snapped); sprite fills the half-tile body inset.
	void DrawUnit(UnitType type, TeamId teamID, UnitFrame frame, Vector2 tileCorner) const;
	// Origin-aligned to the half-tile body center; no-op when the atlas is down
	// or the name is unknown. rotationDeg spins around the body center.
	void DrawAtlasFrame(const std::string &spriteName, Vector2 tileCorner, Color tint,
						float scale = 1.0f, float rotationDeg = 0.0f) const;
	// Screen rect a named sprite occupies, for selection boxes. False when the
	// atlas is down or the name is unknown.
	bool SpriteDrawRect(const std::string &spriteName, Vector2 tileCorner, Rectangle &out,
						float scale = 1.0f) const;
	void DrawBuilding(BuildingType type, TeamId teamID, int tileX, int tileY,
					  BuildingFrame frame = BuildingFrame::Operational,
					  Color tint = WHITE) const;
	void DrawNode(ResourceKind kind, Vector2 center) const;
	void DrawIcon(ResourceKind kind, Vector2 screenPos) const; // 16px HUD icon
	void DrawRallyMarker(TeamId teamID, Vector2 worldCenter) const;
	// False on the headless/rectangle path.
	bool HasFont() const;
	const Font &UiFont() const;
	// Falls back to DrawText when art is down; spacing matches DrawText (size/10).
	static void DrawUiText(const Art *art, const char *text, int x, int y, int size,
						   Color color);
	// 128px, team-neutral; no-op when rectangles are up or the type has no tile.
	void DrawTerrain(TerrainType type, Vector2 corner) const;

	static constexpr int kTileFrames = ArtSprites::kTileFrames;
	static constexpr float kWaterFrameSeconds = ArtSprites::kWaterFrameSeconds;
	static int WaterFrame(float seconds);
	bool UseTileSheets() const;
	static const char *TileSheetFile(TileSheet sheet);
	static std::optional<TileSheet> TileSheetFor(TerrainType type);
	static int TileVariant(int cellX, int cellY);
	// 64px render cell; no-op when rectangles are up or the sheet is down.
	void DrawTileFill(TileSheet sheet, int frame, Vector2 corner) const;

	static constexpr int kTileSides = ArtSprites::kTileSides;
	static constexpr int kBlendPieces = ArtSprites::kBlendPieces;
	static const char *BlendSheetFile(BlendSheet sheet);
	static std::optional<BlendSheet> BlendSheetFor(TileSheet base, TileSheet edge);
	static int TileMaterialRank(TileSheet sheet);
	static int BlendPieceIndex(int sideMask);
	using TileBlendDraw = ArtSprites::TileBlendDraw;
	static int PlanTileBlends(TileSheet base,
							  const TileSheet neighbours[kTileSides],
							  TileBlendDraw out[kTileSides]);
	bool UseTileBlends() const;
	// 64px blend piece; no-op when rectangles are up or the sheet is down.
	void DrawTileBlend(BlendSheet sheet, int piece, Vector2 corner) const;

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

	// Foot infantry render 72px cells against the 128px tile; others 1x.
	static float BaseArtScale(UnitType type);
	// `TerrainFile` is the `<stem>_128px.png` stem; `TerrainSlot` indexes
	// terrain_[] in 0..kTerrainSlots-1.
	static constexpr int kTerrainSlots = ArtSprites::kTerrainSlots;
	static const char *TerrainFile(TerrainType type);
	static int TerrainSlot(TerrainType type);

private:
	ArtSprites sprites_;
	ArtDraw draw_;
};
