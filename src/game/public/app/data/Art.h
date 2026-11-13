#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "raylib.h"
#include "app/data/ArtDraw.h"
#include "app/data/ArtSprites.h"
#include "app/data/InitMode.h"
#include "core/Subsystem.h"
#include "core/Team.h"

enum class TerrainType : std::uint8_t;

// Sprite art + particles + floating damage numbers.

// CPU particle pool: impact sparks, muzzle flashes, death bursts.
// Logic is device-independent (tested); Draw needs a live window.
struct Particle
{
	Vector2 pos = {};
	Vector2 vel = {};
	float life = 0.0f;
	float maxLife = 1.0f;
	float size = 3.0f;
	Color color = WHITE;
};

class Particles
{
public:
	static constexpr std::size_t kMax = 512;

	void SpawnBurst(Vector2 center, Color color, int count, float speed, float life);
	void Update(float dt);
	void Draw() const;
	void Clear();
	std::size_t Count() const;

private:
	std::vector<Particle> live_;
};

// Floating damage numbers: rise-and-fade text spawned once per hit. Same
// pool shape as Particles; logic is device-independent, Draw needs a window.
struct DamageNumber
{
	Vector2 pos = {};
	float value = 0.0f;
	float life = 0.0f;
	float maxLife = 1.0f;
};

class DamageNumbers
{
public:
	static constexpr std::size_t kMax = 256;
	static constexpr float kLife = 0.7f;
	static constexpr float kRiseSpeed = 80.0f; // pixels per second, upward

	void Spawn(Vector2 pos, float value);
	void Update(float dt);
	void Draw() const; // world-space: call inside BeginMode2D/EndMode2D
	void Clear();
	std::size_t Count() const;

private:
	std::vector<DamageNumber> live_;
};

// Facade over the sprite/resolution component (ArtSprites), the draw-call
// component (ArtDraw) and the effect pools. Owns Init/Shutdown order and
// forwards every public call; callers hold this, never the parts.
class Art : public Subsystem
{
public:
	// Load every sprite. Headless or on a missing file, loads
	// nothing and UseRectangles stays true. Returns IsReady.
	bool Init(InitMode mode);
	void Shutdown();

	bool IsReady() const;
	bool UseRectangles() const; // fallback flag: draw colored rects instead

	// tileCorner = unit.position (snapped); sprite fills the half-tile body inset.
	void DrawUnit(UnitType type, TeamId teamID, UnitFrame frame, Vector2 tileCorner) const;
	// Atlas blit of a named sprite, origin-aligned to the half-tile body center;
	// no-op when the atlas is down or the name is unknown. rotationDeg spins
	// the blit around the body center (turret layer, 0 = unrotated).
	void DrawAtlasFrame(const std::string &spriteName, Vector2 tileCorner, Color tint,
						float scale = 1.0f, float rotationDeg = 0.0f) const;
	// Screen rectangle a named sprite occupies when drawn at tileCorner, so a
	// selection box can match art larger than the half-tile body inset. False when
	// the atlas is down or the name is unknown.
	bool SpriteDrawRect(const std::string &spriteName, Vector2 tileCorner, Rectangle &out,
						float scale = 1.0f) const;
	void DrawBuilding(BuildingType type, TeamId teamID, int tileX, int tileY,
					  BuildingFrame frame = BuildingFrame::Operational,
					  Color tint = WHITE) const;
	void DrawNode(ResourceKind kind, Vector2 center) const;
	void DrawIcon(ResourceKind kind, Vector2 screenPos) const; // 16px HUD icon
	void DrawRallyMarker(TeamId teamID, Vector2 worldCenter) const;
	// UI typeface; HasFont is false on the headless/rectangle path.
	bool HasFont() const;
	const Font &UiFont() const;
	// Raw-text draw through the UI typeface; falls back to DrawText when art
	// is down. Spacing matches DrawText (size/10).
	static void DrawUiText(const Art *art, const char *text, int x, int y, int size,
						   Color color);
	// Terrain tile blit (128px, team-neutral); no-op when rectangles are up
	// or the type has no tile.
	void DrawTerrain(TerrainType type, Vector2 corner) const;

	// True once the atlas JSON parsed AND every sheet texture loaded.
	bool UseAtlas() const;
	// Parse the atlas files without touching the GPU (tests).
	[[nodiscard]] std::expected<void, ConfigError> LoadAtlas();
	// Sprite name for a unit at timeSeconds: directional attack (when
	// attacking and the type has attack anims), directional walk/idle anims
	// otherwise, with directional-to-base and static-pose fallbacks. Empty
	// when no sheet or no entries.
	std::string UnitSprite(UnitType type, bool moving, unsigned int id,
						   float timeSeconds, int facingDir, bool attacking = false) const;
	// True when the type has a directional death anim in the atlas.
	bool HasUnitDeathAnim(UnitType type) const;
	// Non-looping death-anim frame for a unit `elapsedSeconds` into its death:
	// clamped to the anim's total duration, holding the last frame past the
	// end. facingDir is a Facing octant (0..7); "" when no death anim exists.
	std::string UnitDeathSprite(UnitType type, unsigned int id, float elapsedSeconds,
								int facingDir) const;
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

	// Base art scale multiplier (foot infantry renders 72px cells down to
	// the 128px tile; everything else at 1x).
	static float BaseArtScale(UnitType type);
	// Terrain filename stem + tile slot per type (`<stem>_128px.png`).
	// Slots index terrain_[], so they run 0..kTerrainSlots-1.
	static constexpr int kTerrainSlots = ArtSprites::kTerrainSlots;
	static const char *TerrainFile(TerrainType type);
	static int TerrainSlot(TerrainType type);

	Particles &ParticlesPool();
	const Particles &ParticlesPool() const;

private:
	ArtSprites sprites_;
	ArtDraw draw_;
	Particles particles_;
};
