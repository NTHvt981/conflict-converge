#include "SpriteData.h"

#include <cstdio> // snprintf for grid sprite names
#include <fstream>
#include <sstream>
#include <unordered_set>

#include <google/protobuf/util/json_util.h>

#include "spritedata.pb.h"

namespace
{

bool ReadWholeFile(const std::string &path, std::string &out)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

bool RectInside(int left, int top, int right, int bottom, int width, int height)
{
    return left >= 0 && top >= 0 && left < right && top < bottom && right <= width &&
           bottom <= height;
}

} // namespace

bool ParseSpriteSheetJson(const std::string &json, SpriteSheetData &out)
{
    cc::sprites::SpriteSheet proto;
    if (!google::protobuf::util::JsonStringToMessage(json, &proto).ok())
    {
        return false;
    }

    SpriteSheetData sheet;

    // --- textures: unique ids, non-empty files, positive dims ---
    std::unordered_set<int> textureIds;
    for (const cc::sprites::SpriteTexture &t : proto.textures())
    {
        if (t.file().empty() || t.width() <= 0 || t.height() <= 0 ||
            !textureIds.insert(t.id()).second)
        {
            return false;
        }
        SpriteTextureInfo info;
        info.id = t.id();
        info.file = t.file();
        info.width = t.width();
        info.height = t.height();
        sheet.textures.push_back(info);
    }

    auto findTexture = [&](int id) -> const SpriteTextureInfo * {
        for (const SpriteTextureInfo &t : sheet.textures)
        {
            if (t.id == id)
            {
                return &t;
            }
        }
        return nullptr;
    };

    std::unordered_set<int> spriteIds;
    std::unordered_set<std::string> spriteNames;

    auto pushSprite = [&](const std::string &name, int id, int texture,
                          const SpriteRect &bounds, const SpriteOriginPx &origin,
                          int maskSprite) -> bool {
        if (name.empty() || !spriteIds.insert(id).second || !spriteNames.insert(name).second)
        {
            return false;
        }
        const SpriteTextureInfo *tex = findTexture(texture);
        if (tex == nullptr ||
            !RectInside(bounds.left, bounds.top, bounds.right, bounds.bottom, tex->width,
                        tex->height))
        {
            return false;
        }
        SpriteDefInfo info;
        info.name = name;
        info.id = id;
        info.texture = texture;
        info.bounds = bounds;
        info.origin = origin;
        // Stored unvalidated: the mask sprite may be pushed later in load
        // order (a later grid), so refs resolve in the pass after both loops.
        info.maskSprite = maskSprite;
        sheet.sprites.push_back(info);
        return true;
    };

    // --- explicit sprites ---
    for (const cc::sprites::SpriteDef &s : proto.sprites())
    {
        const SpriteRect bounds{ s.bounds().left(), s.bounds().top(), s.bounds().right(),
                                s.bounds().bottom() };
        const SpriteOriginPx origin{ s.origin().x(), s.origin().y() };
        if (!pushSprite(s.name(), s.id(), s.texture(), bounds, origin, s.mask_sprite()))
        {
            return false;
        }
    }

    // --- grids: expand to "<prefix>_<r>_<c>", ids start_id + r*cols + c ---
    for (const cc::sprites::SpriteGrid &g : proto.grids())
    {
        if (g.prefix().empty() || g.rows() <= 0 || g.cols() <= 0 || g.cell_w() <= 0 ||
            g.cell_h() <= 0)
        {
            return false;
        }
        const SpriteTextureInfo *tex = findTexture(g.texture());
        if (tex == nullptr || g.cols() * g.cell_w() > tex->width ||
            g.rows() * g.cell_h() > tex->height)
        {
            return false;
        }
        const SpriteOriginPx origin{ g.origin().x(), g.origin().y() };
        for (int r = 0; r < g.rows(); ++r)
        {
            for (int c = 0; c < g.cols(); ++c)
            {
                char name[128];
                std::snprintf(name, sizeof(name), "%s_%d_%d", g.prefix().c_str(), r, c);
                const SpriteRect bounds{ c * g.cell_w(), r * g.cell_h(), (c + 1) * g.cell_w(),
                                         (r + 1) * g.cell_h() };
                const int maskId = g.mask_grid_start_id() != 0
                                       ? g.mask_grid_start_id() + r * g.cols() + c
                                       : 0;
                if (!pushSprite(name, g.start_id() + r * g.cols() + c, g.texture(), bounds,
                                origin, maskId))
                {
                    return false;
                }
            }
        }
    }

    // --- mask refs: must resolve to a same-sized sprite (pixel-aligned
    // composite). Runs after both loops since a mask may be declared later
    // in load order than its base (same reason animation frames validate
    // late).
    for (const SpriteDefInfo &s : sheet.sprites)
    {
        if (s.maskSprite == 0)
        {
            continue;
        }
        const SpriteDefInfo *mask = FindSpriteById(sheet, s.maskSprite);
        if (mask == nullptr)
        {
            return false; // dangling mask ref
        }
        const int w = s.bounds.right - s.bounds.left;
        const int h = s.bounds.bottom - s.bounds.top;
        const int mw = mask->bounds.right - mask->bounds.left;
        const int mh = mask->bounds.bottom - mask->bounds.top;
        if (w != mw || h != mh)
        {
            return false; // base/mask must composite pixel-aligned
        }
    }

    // --- animations: unique names/ids, >= 1 frame, live sprite refs ---
    std::unordered_set<int> animIds;
    std::unordered_set<std::string> animNames;
    for (const cc::sprites::SpriteAnim &a : proto.animations())
    {
        if (a.name().empty() || !animIds.insert(a.id()).second ||
            !animNames.insert(a.name()).second || a.frames_size() == 0)
        {
            return false;
        }
        SpriteAnimInfo info;
        info.name = a.name();
        info.id = a.id();
        info.loop = a.loop();
        for (const cc::sprites::AnimFrame &f : a.frames())
        {
            if (f.duration_ms() <= 0 || FindSpriteById(sheet, f.sprite()) == nullptr)
            {
                return false;
            }
            SpriteAnimFrame frame;
            frame.sprite = f.sprite();
            frame.durationMs = f.duration_ms();
            info.frames.push_back(frame);
        }
        sheet.animations.push_back(info);
    }

    out = sheet;
    return true;
}

bool LoadSpriteSheet(const std::string &path, SpriteSheetData &out)
{
    std::string json;
    if (!ReadWholeFile(path, json))
    {
        return false;
    }
    return ParseSpriteSheetJson(json, out);
}

const SpriteDefInfo *FindSpriteByName(const SpriteSheetData &sheet, const std::string &name)
{
    for (const SpriteDefInfo &s : sheet.sprites)
    {
        if (s.name == name)
        {
            return &s;
        }
    }
    return nullptr;
}

const SpriteDefInfo *FindSpriteById(const SpriteSheetData &sheet, int id)
{
    for (const SpriteDefInfo &s : sheet.sprites)
    {
        if (s.id == id)
        {
            return &s;
        }
    }
    return nullptr;
}

const SpriteAnimInfo *FindAnimByName(const SpriteSheetData &sheet, const std::string &name)
{
    for (const SpriteAnimInfo &a : sheet.animations)
    {
        if (a.name == name)
        {
            return &a;
        }
    }
    return nullptr;
}

const SpriteAnimInfo *FindAnimById(const SpriteSheetData &sheet, int id)
{
    for (const SpriteAnimInfo &a : sheet.animations)
    {
        if (a.id == id)
        {
            return &a;
        }
    }
    return nullptr;
}
