#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "raylib.h"
#include "Building.h"
#include "Nodes.h"
#include "SpriteData.h"
#include "Subsystem.h"
#include "Unit.h"

enum class TerrainType : std::uint8_t;

// Sprite art + particles + floating damage numbers. Assets live under
// data/sprites/ and data/fonts/ (see Art.h.context.md for the contract);
// Init(false) is the headless rectangle-fallback path.

// AttackPhase -> sprite frame. WindUp telegraphs and Recover follows
// through on the attack frame; everything else idles. Pure: unit-tested.
enum class UnitFrame
{
    Idle,
    Attack
};
UnitFrame FrameForPhase(AttackPhase phase);

// Rifle-infantry squad visual. Returns the number of soldier sprites to draw
// (driven by healthFraction, capped per type) and fills outOffsets with that
// many pixel offsets from the unit's tile-corner position. Deterministic
// per-entity. outScale shrinks with the count so clustered soldiers don't
// overlap; pass it to DrawAtlasFrame.
int SquadSlots(UnitType type, unsigned int id, float healthFraction,
               std::array<Vector2, 6> &outOffsets, float &outScale);

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
    static constexpr float kRiseSpeed = 40.0f; // pixels per second, upward

    void Spawn(Vector2 pos, float value);
    void Update(float dt);
    void Draw() const; // world-space: call inside BeginMode2D/EndMode2D
    void Clear();
    std::size_t Count() const;

private:
    std::vector<DamageNumber> live_;
};

class Art : public Subsystem
{
public:
    // Load every sprite. With withDevice=false or on a missing file, loads
    // nothing and UseRectangles stays true. Returns IsReady.
    bool Init(bool withDevice);
    void Shutdown();

    bool IsReady() const;
    bool UseRectangles() const; // fallback flag: draw colored rects instead

    // tileCorner = unit.position (snapped); sprite fills the 32x32 body inset.
    void DrawUnit(UnitType type, int teamID, UnitFrame frame, Vector2 tileCorner) const;
    // Atlas blit of a named sprite, origin-aligned to the 32x32 body center;
    // no-op when the atlas is down or the name is unknown. rotationDeg spins
    // the blit around the body center (M3 render: turret layer, 0 = legacy).
    void DrawAtlasFrame(const std::string &spriteName, Vector2 tileCorner, Color tint,
                        float scale = 1.0f, float rotationDeg = 0.0f) const;
    void DrawBuilding(BuildingType type, int teamID, int tileX, int tileY) const;
    void DrawNode(ResourceKind kind, Vector2 center) const;
    void DrawIcon(ResourceKind kind, Vector2 screenPos) const; // 16px HUD icon
    // UI typeface; HasFont is false on the headless/rectangle path.
    bool HasFont() const;
    const Font &UiFont() const;
    // Raw-text draw through the UI typeface; falls back to DrawText when art
    // is down. Spacing matches DrawText (size/10).
    static void DrawUiText(const Art *art, const char *text, int x, int y, int size,
                           Color color);
    // Terrain tile blit (64px, team-neutral); no-op when rectangles are up
    // or the type has no tile.
    void DrawTerrain(TerrainType type, Vector2 corner) const;

    // True once the atlas JSON parsed AND every sheet texture loaded.
    bool UseAtlas() const;
    // Parse the atlas files without touching the GPU (tests). False on
    // missing/invalid files.
    bool LoadAtlas();
    // Sprite name for a unit at timeSeconds: directional attack (when
    // attacking and the type has attack anims), walk/idle otherwise, with
    // legacy fallbacks. Empty when no sheet or no entries.
    std::string UnitSprite(UnitType type, bool moving, unsigned int id,
                           float timeSeconds, int facingDir, bool attacking = false) const;
    // Base art scale multiplier (prototype infantry renders at 2x).
    static float BaseArtScale(UnitType type);
    // Terrain filename stem + tile slot per type (`<stem>_64px.png`).
    static const char *TerrainFile(TerrainType type);
    static int TerrainSlot(TerrainType type);
    // Team color for atlas mask tinting (mask layer only); color-blind mode
    // swaps in the Okabe-Ito pair.
    Color TeamTint(int teamID) const;
    void SetColorBlindMode(bool enabled);

    Particles &ParticlesPool();
    const Particles &ParticlesPool() const;

private:
    static int TeamSlot(int teamID); // 0 -> blue, anything else -> red
    void DrawOneAtlasSprite(const SpriteDefInfo &sprite, Vector2 tileCorner, Color tint,
                            float scale, float rotationDeg = 0.0f) const;
    bool ready_ = false;
    bool fallback_ = true;
    bool atlasReady_ = false;
    bool sheetLoaded_ = false;
    bool colorBlindMode_ = false; // Okabe-Ito team palette when true
    SpriteSheetData sheet_;
    std::unordered_map<std::string, Texture2D> atlas_; // SpriteTextureInfo.id -> sheet
    Particles particles_;
    Texture2D units_[static_cast<int>(UnitType::Count)][2][2] = {}; // [type][team][frame]
    Texture2D buildings_[3][2] = {}; // [type][team]
    Texture2D nodes_[2] = {};        // [kind]
    Texture2D icons_[2] = {};        // [kind]
    Texture2D terrain_[4] = {};      // [TerrainSlot]: grass/water/forest/rock
    Font uiFont_ = {};               // Porto Buena (valid only when fontReady_)
    bool fontReady_ = false;
};
