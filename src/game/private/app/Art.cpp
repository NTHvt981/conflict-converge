#include "Art.h"

#include <cmath>
#include <cstdio> // snprintf for sprite paths
#include <cstdlib>

#include "MathUtils.h" // cc::TILE_SIZE

UnitFrame FrameForPhase(AttackPhase phase)
{
    switch (phase)
    {
    case AttackPhase::WindUp:
    case AttackPhase::Recover:
        return UnitFrame::Attack;
    case AttackPhase::Ready:
        break;
    }
    return UnitFrame::Idle;
}

// Phase 14: deterministic hash for per-entity jitter (no RNG, no save state).
static unsigned int HashId(unsigned int id)
{
    id = ((id >> 16) ^ id) * 0x45d9f3b;
    id = ((id >> 16) ^ id) * 0x45d9f3b;
    id = (id >> 16) ^ id;
    return id;
}

int SquadSlots(UnitType type, unsigned int id, float healthFraction,
               std::array<Vector2, 6> &outOffsets, float &outScale)
{
    // Only foot types with multi-soldier visuals get the squad treatment.
    // Engineers render as a single centered sprite (like vehicles).
    if (type != UnitType::Infantry && type != UnitType::AntiArmorInfantry)
    {
        outOffsets[0] = { 0.0f, 0.0f };
        outScale = 1.0f;
        return 1;
    }

    // Soldier count from health, scaled to each type's max squad size:
    // Infantry 5 at >80%, 4 at >60%, 3 at >40%, 2 at >20%, 1 otherwise;
    // AntiArmorInfantry fights as a 2-man team, dropping to 1 below half.
    int count;
    if (type == UnitType::Infantry)
    {
        if (healthFraction > 0.80f)
            count = 5;
        else if (healthFraction > 0.60f)
            count = 4;
        else if (healthFraction > 0.40f)
            count = 3;
        else if (healthFraction > 0.20f)
            count = 2;
        else
            count = 1;
    }
    else
    {
        count = (healthFraction > 0.50f) ? 2 : 1;
    }

    // Per-count (scale, offsets): a 64x64 tile cannot fit multiple native
    // 32px soldiers side by side (the old single 8px-spaced table overlapped
    // ~75% per sprite), so each row shrinks the blit and spreads the slots
    // so adjacent centers stay >= scale * 32px apart while the cluster stays
    // near the tile's half-size. Index by count - 1; rows sized for the
    // count (unused tail slots stay {0,0}).
    struct SquadLayout
    {
        float scale;
        float offsets[6][2];
    };
    static constexpr SquadLayout kLayouts[6] = {
        { 1.00f, { { 0, 0 } } },
        { 0.80f, { { -14, 0 }, { 14, 0 } } },
        { 0.70f, { { 0, -14 }, { -13, 10 }, { 13, 10 } } },
        { 0.65f, { { -13, -13 }, { 13, -13 }, { -13, 13 }, { 13, 13 } } },
        { 0.55f, { { 0, 0 }, { -15, -15 }, { 15, -15 }, { -15, 15 }, { 15, 15 } } },
        { 0.50f,
          { { -18, -17 }, { 0, -17 }, { 18, -17 }, { -18, 17 }, { 0, 17 }, { 18, 17 } } },
    };
    const SquadLayout &layout = kLayouts[count - 1];
    outScale = layout.scale;

    const unsigned int h = HashId(id);
    // Jitter scales down with the blit so it can't eat the thinner margins
    // at higher counts (was a fixed ±2px against 16px sprites).
    const float jitterPx = 2.0f * layout.scale;
    for (int i = 0; i < count; ++i)
    {
        // Deterministic jitter derived from id + slot index.
        const unsigned int jitter = HashId(h + static_cast<unsigned int>(i));
        const float jx = (static_cast<float>(jitter & 0x7) / 7.0f - 0.5f) * 2.0f * jitterPx;
        const float jy =
            (static_cast<float>((jitter >> 3) & 0x7) / 7.0f - 0.5f) * 2.0f * jitterPx;
        outOffsets[i] = { layout.offsets[i][0] + jx, layout.offsets[i][1] + jy };
    }
    return count;
}

void Particles::SpawnBurst(Vector2 center, Color color, int count, float speed, float life)
{
    for (int i = 0; i < count && live_.size() < kMax; ++i)
    {
        const float angle =
            static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 2.0f * 3.14159265f;
        const float magnitude = speed * (0.4f + 0.6f * static_cast<float>(rand()) /
                                                   static_cast<float>(RAND_MAX));
        Particle p;
        p.pos = center;
        p.vel = { std::cos(angle) * magnitude, std::sin(angle) * magnitude };
        p.life = life;
        p.maxLife = life > 0.0f ? life : 1.0f;
        p.size = 2.0f + 2.0f * static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        p.color = color;
        live_.push_back(p);
    }
}

void Particles::Update(float dt)
{
    for (std::size_t i = 0; i < live_.size();)
    {
        Particle &p = live_[i];
        p.life -= dt;
        if (p.life <= 0.0f)
        {
            p = live_.back();
            live_.pop_back();
            continue;
        }
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;
        ++i;
    }
}

void Particles::Draw() const
{
    for (const Particle &p : live_)
    {
        const float alpha = p.life / p.maxLife;
        DrawCircleV(p.pos, p.size, Fade(p.color, alpha > 1.0f ? 1.0f : alpha));
    }
}

void Particles::Clear()
{
    live_.clear();
}

std::size_t Particles::Count() const
{
    return live_.size();
}

namespace
{

const char *UnitFile(UnitType type)
{
    switch (type)
    {
    case UnitType::Infantry:
        return "infantry";
    case UnitType::AntiArmorInfantry:
        return "antiarmor";
    case UnitType::Engineer:
        return "engineer";
    case UnitType::IFV:
        return "ifv";
    case UnitType::Artillery:
        return "artillery";
    case UnitType::LightTank:
        return "lighttank";
    case UnitType::HeavyTank:
        return "heavytank";
    }
    return "infantry"; // unreachable; keeps MSVC from warning
}

const char *BuildingFile(BuildingType type)
{
    switch (type)
    {
    case BuildingType::Base:
        return "base";
    case BuildingType::ResourceDepot:
        return "depot";
    case BuildingType::Factory:
        return "factory";
    }
    return "depot"; // unreachable
}

const char *NodeFile(ResourceKind kind)
{
    return kind == ResourceKind::Iron ? "iron" : "oil";
}

const char *TeamName(int teamID)
{
    return teamID == 0 ? "blue" : "red";
}

} // namespace

int Art::TeamSlot(int teamID)
{
    return teamID == 0 ? 0 : 1;
}

bool Art::Init(bool withDevice)
{
    Shutdown();
    if (!withDevice)
    {
        return false; // headless/test path: rectangles forever
    }
    fallback_ = false;
    char path[128];
    for (int t = 0; t < 7; ++t)
    {
        for (int team = 0; team < 2; ++team)
        {
            for (int f = 0; f < 2; ++f)
            {
                std::snprintf(path, sizeof(path), "data/sprites/units/%s_%s_%s.png",
                              UnitFile(static_cast<UnitType>(t)), team == 0 ? "blue" : "red",
                              f == 0 ? "idle" : "attack");
                units_[t][team][f] = LoadTexture(path);
                if (units_[t][team][f].id == 0)
                {
                    fallback_ = true;
                }
            }
        }
    }
    for (int t = 0; t < 3; ++t)
    {
        for (int team = 0; team < 2; ++team)
        {
            std::snprintf(path, sizeof(path), "data/sprites/buildings/%s_%s.png",
                          BuildingFile(static_cast<BuildingType>(t)), team == 0 ? "blue" : "red");
            buildings_[t][team] = LoadTexture(path);
            if (buildings_[t][team].id == 0)
            {
                fallback_ = true;
            }
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
    // Atlas override: sprites.json + sheet PNGs. Missing files just leave
    // the atlas down (legacy/rectangle paths are unaffected).
    atlasReady_ = LoadAtlas();
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
    ready_ = !fallback_;
    return ready_;
}

void Art::Shutdown()
{
    for (int t = 0; t < 7; ++t)
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
    for (int t = 0; t < 3; ++t)
    {
        for (int team = 0; team < 2; ++team)
        {
            if (buildings_[t][team].id != 0)
            {
                UnloadTexture(buildings_[t][team]);
                buildings_[t][team] = {};
            }
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
    for (const auto &entry : atlas_)
    {
        UnloadTexture(entry.second);
    }
    atlas_.clear();
    sheet_ = SpriteSheetData{};
    atlasReady_ = false;
    sheetLoaded_ = false;
    particles_.Clear();
    ready_ = false;
    fallback_ = true;
}

bool Art::IsReady() const
{
    return ready_;
}

bool Art::UseRectangles() const
{
    return fallback_;
}

Particles &Art::ParticlesPool()
{
    return particles_;
}

const Particles &Art::ParticlesPool() const
{
    return particles_;
}

void Art::DrawUnit(UnitType type, int teamID, UnitFrame frame, Vector2 tileCorner) const
{
    if (fallback_)
    {
        return;
    }
    const Texture2D tex = units_[static_cast<int>(type)][TeamSlot(teamID)][static_cast<int>(frame)];
    DrawTextureV(tex, { tileCorner.x + 16.0f, tileCorner.y + 16.0f }, WHITE);
}

bool Art::UseAtlas() const
{
    return atlasReady_;
}

bool Art::LoadAtlas()
{
    SpriteSheetData sheet;
    if (!LoadSpriteSheet("data/sprites.json", sheet))
    {
        return false;
    }
    sheet_ = sheet;
    sheetLoaded_ = true;
    return true;
}

std::string Art::UnitSprite(UnitType type, bool moving, unsigned int id,
                            float timeSeconds) const
{
    if (!sheetLoaded_)
    {
        return {};
    }
    const std::string prefix = UnitFile(type);
    if (moving)
    {
        // "<type>_walk" animation frame at timeSeconds; per-entity offset so
        // squad soldiers don't march in sync. Falls through to idle when the
        // type has no walk animation.
        if (const SpriteAnimInfo *anim = FindAnimByName(sheet_, prefix + "_walk");
            anim != nullptr && !anim->frames.empty())
        {
            int total = 0;
            for (const SpriteAnimFrame &f : anim->frames)
            {
                total += f.durationMs;
            }
            if (total > 0)
            {
                int t = static_cast<int>(timeSeconds * 1000.0f) +
                        static_cast<int>(id * 37u % static_cast<unsigned int>(total));
                t %= total;
                if (t < 0)
                {
                    t += total;
                }
                for (const SpriteAnimFrame &f : anim->frames)
                {
                    if (t < f.durationMs)
                    {
                        if (const SpriteDefInfo *s = FindSpriteById(sheet_, f.sprite);
                            s != nullptr)
                        {
                            return s->name;
                        }
                        break;
                    }
                    t -= f.durationMs;
                }
            }
        }
    }
    if (const SpriteDefInfo *idle = FindSpriteByName(sheet_, prefix + "_idle_0_0");
        idle != nullptr)
    {
        return idle->name;
    }
    return {};
}

Color Art::TeamTint(int teamID) const
{
    if (colorBlindMode_)
    {
        // Okabe-Ito colorblind-safe pair: orange / sky blue.
        return teamID == 0 ? Color{ 230, 159, 0, 255 } : Color{ 86, 180, 233, 255 };
    }
    return teamID == 0 ? BLUE : RED;
}

void Art::SetColorBlindMode(bool enabled)
{
    colorBlindMode_ = enabled;
}

void Art::DrawAtlasFrame(const std::string &spriteName, Vector2 tileCorner, Color tint,
                           float scale) const
{
    if (!atlasReady_ || spriteName.empty())
    {
        return;
    }
    const SpriteDefInfo *s = FindSpriteByName(sheet_, spriteName);
    if (s == nullptr)
    {
        return;
    }
    if (s->maskSprite == 0)
    {
        // No mask: full-sprite multiply tint (correct for all-white art).
        DrawOneAtlasSprite(*s, tileCorner, tint, scale);
        return;
    }
    // Base layer at true colors, mask layer team-tinted on top.
    DrawOneAtlasSprite(*s, tileCorner, WHITE, scale);
    if (const SpriteDefInfo *mask = FindSpriteById(sheet_, s->maskSprite); mask != nullptr)
    {
        DrawOneAtlasSprite(*mask, tileCorner, tint, scale);
    }
}

void Art::DrawOneAtlasSprite(const SpriteDefInfo &s, Vector2 tileCorner, Color tint,
                             float scale) const
{
    const auto it = atlas_.find(s.texture);
    if (it == atlas_.end() || it->second.id == 0)
    {
        return;
    }
    const float w = static_cast<float>(s.bounds.right - s.bounds.left);
    const float h = static_cast<float>(s.bounds.bottom - s.bounds.top);
    const Rectangle src = { static_cast<float>(s.bounds.left), static_cast<float>(s.bounds.top),
                            w, h };
    // The sprite origin lands on the 32x32 body center (tileCorner + 32);
    // scale shrinks the blit around that anchor so squad clusters separate.
    const Vector2 anchor = { tileCorner.x + 32.0f, tileCorner.y + 32.0f };
    const Rectangle dest = { anchor.x - static_cast<float>(s.origin.x) * scale,
                             anchor.y - static_cast<float>(s.origin.y) * scale, w * scale,
                             h * scale };
    DrawTexturePro(it->second, src, dest, { 0.0f, 0.0f }, 0.0f, tint);
}

void Art::DrawBuilding(BuildingType type, int teamID, int tileX, int tileY) const
{
    if (fallback_)
    {
        return;
    }
    const Vector2 corner = cc::ToRaylib(cc::TileToWorld(tileX, tileY));
    DrawTextureV(buildings_[static_cast<int>(type)][TeamSlot(teamID)], corner, WHITE);
}

void Art::DrawNode(ResourceKind kind, Vector2 center) const
{
    if (fallback_)
    {
        return;
    }
    const Texture2D tex = nodes_[static_cast<int>(kind)];
    DrawTextureV(tex, { center.x - 16.0f, center.y - 16.0f }, WHITE);
}

void Art::DrawIcon(ResourceKind kind, Vector2 screenPos) const
{
    if (fallback_)
    {
        return;
    }
    DrawTextureV(icons_[static_cast<int>(kind)], screenPos, WHITE);
}
