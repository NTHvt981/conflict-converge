#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "raylib.h" // Texture2D, Vector2, Color (device-independent types)
#include "Building.h" // BuildingType for DrawBuilding
#include "Nodes.h"    // ResourceKind for DrawNode/DrawIcon
#include "SpriteData.h" // SpriteSheetData for the JSON atlas
#include "Unit.h"     // UnitType, AttackPhase

// M12: sprite art + particles. Filenames under data/sprites/ are the
// contract (tools/gen_sprites.py produces them; AI-generated sheets drop in
// later): units/<type>_<blue|red>_<idle|attack>.png (32px),
// buildings/<base|factory|depot>_<blue|red>.png (64/64/32px),
// nodes/<iron|oil>.png (32px), icons/<iron|oil>.png (16px).
// Atlas override: data/sprites.json (schema in proto/spritedata.proto)
// names source-rects inside sheet PNGs (e.g. placeholder infantry 8x8
// grids). Types with atlas entries render from the atlas (team-tinted, since
// sheet art is white); types without fall through to the filename path.
// Headless-safe: Init(false) loads nothing and UseRectangles() reports true,
// so tests and the fallback path never touch the GPU.

// AttackPhase -> sprite frame. WindUp telegraphs and Recover follows
// through on the attack frame; everything else idles. Pure: unit-tested.
enum class UnitFrame
{
    Idle,
    Attack
};
UnitFrame FrameForPhase(AttackPhase phase);

// Phase 14: Infantry squad visual. Returns the number of soldier sprites
// to draw (driven by healthFraction, capped per type: 5 for Infantry, 2 for
// AntiArmorInfantry, 1 for Engineers and vehicles) and fills outOffsets with
// that many Vector2 pixel offsets from the unit's tile-corner position.
// Deterministic per-entity (same id always produces the same layout) so it
// doesn't need to be stored or included in save data.
// Returns count=1 with outOffsets[0]={0,0} for non-infantry types.
int SquadSlots(UnitType type, unsigned int id, float healthFraction,
               std::array<Vector2, 6> &outOffsets);

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

class Art
{
public:
    // Load every sprite. With withDevice=false (tests) or on any missing
    // file, loads nothing and UseRectangles() stays true. Returns IsReady().
    bool Init(bool withDevice);
    void Shutdown();

    bool IsReady() const;
    bool UseRectangles() const; // fallback flag: draw colored rects instead

    // tileCorner = unit.position (snapped); sprite fills the 32x32 body inset.
    void DrawUnit(UnitType type, int teamID, UnitFrame frame, Vector2 tileCorner) const;
    // Atlas override: source-rect blit of a named sprite from sprites.json,
    // origin-aligned to the 32x32 body center. No-op when the atlas is down
    // or the name is unknown (headless-safe).
    void DrawAtlasFrame(const std::string &spriteName, Vector2 tileCorner, Color tint) const;
    void DrawBuilding(BuildingType type, int teamID, int tileX, int tileY) const;
    void DrawNode(ResourceKind kind, Vector2 center) const;
    void DrawIcon(ResourceKind kind, Vector2 screenPos) const; // 16px HUD icon

    // Atlas status: true once sprites.json parsed AND every sheet texture
    // loaded (Init(true) path). Headless-safe query.
    bool UseAtlas() const;
    // Parse data/sprites.json without touching the GPU (tests, LoadAtlas
    // seam). False when the file is missing or invalid.
    bool LoadAtlas();
    // Sprite name for a unit at timeSeconds (GetTime() at the call site):
    // "<type>_walk" animation frame while moving (id-offset so squads don't
    // march in sync), "<type>_idle_0_0" otherwise. Empty when no sheet is
    // loaded or the type has no entries — caller falls back to DrawUnit.
    // Pure logic (works headless after LoadAtlas, no GPU), unit-tested.
    std::string UnitSprite(UnitType type, bool moving, unsigned int id,
                           float timeSeconds) const;
    // Team color for atlas tinting (sheet art is white): BLUE/RED, matching
    // the rectangle-fallback body colors.
    static Color TeamTint(int teamID);

    Particles &ParticlesPool();
    const Particles &ParticlesPool() const;

private:
    static int TeamSlot(int teamID); // 0 -> blue, anything else -> red
    bool ready_ = false;
    bool fallback_ = true;
    bool atlasReady_ = false;
    bool sheetLoaded_ = false;
    SpriteSheetData sheet_;
    std::unordered_map<int, Texture2D> atlas_; // SpriteTextureInfo.id -> sheet
    Particles particles_;
    Texture2D units_[7][2][2] = {}; // [type][team][frame]
    Texture2D buildings_[3][2] = {}; // [type][team]
    Texture2D nodes_[2] = {};        // [kind]
    Texture2D icons_[2] = {};        // [kind]
};
