#pragma once

#include <array>
#include <string>

#include "raylib.h"
#include "app/data/ArtSprites.h"
#include "core/Team.h"
#include "economy/Building.h"
#include "economy/Nodes.h"
#include "units/Unit.h"

enum class TerrainType : std::uint8_t;

// AttackPhase -> sprite frame. WindUp telegraphs and Recover follows
// through on the attack frame; everything else idles. Pure: unit-tested.
enum class UnitFrame
{
	Idle,
	Attack
};
UnitFrame FrameForPhase(AttackPhase phase);

// Rifle-infantry squad visual. count is the number of soldier sprites to draw
// (driven by healthFraction, capped per type); slots holds that many pixel
// offsets from the unit's tile-corner position. Deterministic per-entity.
// scale shrinks with the count so clustered soldiers don't overlap; pass it to
// DrawAtlasFrame.
struct SquadLayout
{
	std::array<Vector2, 6> slots = {};
	int count = 0;
	float scale = 1.0f;
};
SquadLayout SquadSlots(UnitType type, unsigned int id, float healthFraction);

// Draw calls over the sprite assets. Every method reads the shared
// ArtSprites state passed in; Art forwards its public drawing API here.
class ArtDraw
{
public:
	// tileCorner = unit.position (snapped); sprite fills the 32x32 body inset.
	void DrawUnit(const ArtSprites &sprites, UnitType type, TeamId teamID,
				  UnitFrame frame, Vector2 tileCorner) const;
	// Atlas blit of a named sprite, origin-aligned to the 32x32 body center;
	// no-op when the atlas is down or the name is unknown. rotationDeg spins
	// the blit around the body center (turret layer, 0 = unrotated).
	void DrawAtlasFrame(const ArtSprites &sprites, const std::string &spriteName,
						Vector2 tileCorner, Color tint, float scale = 1.0f,
						float rotationDeg = 0.0f) const;
	// Screen rectangle a named sprite occupies when drawn at tileCorner, so a
	// selection box can match art larger than the 32x32 body inset. False when
	// the atlas is down or the name is unknown.
	bool SpriteDrawRect(const ArtSprites &sprites, const std::string &spriteName,
						Vector2 tileCorner, Rectangle &out, float scale = 1.0f) const;
	void DrawBuilding(const ArtSprites &sprites, BuildingType type, TeamId teamID,
					  int tileX, int tileY,
					  BuildingFrame frame = BuildingFrame::Operational,
					  Color tint = WHITE) const;
	void DrawNode(const ArtSprites &sprites, ResourceKind kind, Vector2 center) const;
	void DrawIcon(const ArtSprites &sprites, ResourceKind kind,
				  Vector2 screenPos) const; // 16px HUD icon
	void DrawRallyMarker(const ArtSprites &sprites, TeamId teamID,
						 Vector2 worldCenter) const;
	// Terrain tile blit (64px, team-neutral); no-op when rectangles are up
	// or the type has no tile.
	void DrawTerrain(const ArtSprites &sprites, TerrainType type, Vector2 corner) const;

	// UI typeface lookups; HasFont is false on the headless/rectangle path.
	bool HasFont(const ArtSprites &sprites) const;
	const Font &UiFont(const ArtSprites &sprites) const;

	// Raw-text draw through the UI typeface; falls back to DrawText when art
	// is down. Spacing matches DrawText (size/10). nullptr sprites always
	// takes the fallback.
	static void DrawUiText(const ArtSprites *sprites, const char *text, int x, int y,
						   int size, Color color);

private:
	void DrawOneAtlasSprite(const ArtSprites &sprites, const SpriteDefInfo &sprite,
							Vector2 tileCorner, Color tint, float scale,
							float rotationDeg = 0.0f) const;
};
