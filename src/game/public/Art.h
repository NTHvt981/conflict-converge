#pragma once

#include <cstddef>
#include <vector>

#include "raylib.h" // Texture2D, Vector2, Color (device-independent types)
#include "Building.h" // BuildingType for DrawBuilding
#include "Nodes.h"    // ResourceKind for DrawNode/DrawIcon
#include "Unit.h"     // UnitType, AttackPhase

// M12: sprite art + particles. Filenames under data/sprites/ are the
// contract (tools/gen_sprites.py produces them; AI-generated sheets drop in
// later): units/<type>_<blue|red>_<idle|attack>.png (32px),
// buildings/<base|factory|depot>_<blue|red>.png (64/64/32px),
// nodes/<iron|oil>.png (32px), icons/<iron|oil>.png (16px).
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
    void DrawBuilding(BuildingType type, int teamID, int tileX, int tileY) const;
    void DrawNode(ResourceKind kind, Vector2 center) const;
    void DrawIcon(ResourceKind kind, Vector2 screenPos) const; // 16px HUD icon

    Particles &ParticlesPool();
    const Particles &ParticlesPool() const;

private:
    static int TeamSlot(int teamID); // 0 -> blue, anything else -> red
    bool ready_ = false;
    bool fallback_ = true;
    Particles particles_;
    Texture2D units_[7][2][2] = {}; // [type][team][frame]
    Texture2D buildings_[3][2] = {}; // [type][team]
    Texture2D nodes_[2] = {};        // [kind]
    Texture2D icons_[2] = {};        // [kind]
};
