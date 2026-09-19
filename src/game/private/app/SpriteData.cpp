#include "SpriteData.h"

// Fail-closed JSON parsing: cereal signals missing keys via exception, but
// mistyped scalars and malformed documents trip rapidjson's C assert
// (abort in Debug, garbage in Release). The documented override below turns
// every rapidjson complaint into an exception the parse boundary catches,
// so malformed input returns false exactly like the protobuf-JSON path did.
// TU-local by necessity (preprocessor): must precede every cereal include,
// and SpriteData.h must stay cereal-free.

#define CEREAL_RAPIDJSON_ASSERT(x) \
    do { if (!(x)) throw std::runtime_error("malformed sprite JSON"); } while (0)

#include <cereal/archives/json.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

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

// Wire structs mirroring proto/spritedata.proto field-for-field (key names
// match the shipped camelCase JSON). Loading is tolerant by design: every
// member goes through TryLoadValue, so an absent key keeps its default —
// exactly protobuf-JSON's default-instance semantics, which the validation
// below (and its tests) rely on. Only load() is defined: the game never
// writes these files back.
template <class Archive, class T>
void TryLoadValue(Archive &ar, const char *name, T &out)
{
    try
    {
        ar(cereal::make_nvp(name, out));
    }
    catch (const cereal::Exception &)
    {
    }
}

struct JsonBounds
{
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    template <class Archive>
    void load(Archive &ar)
    {
        TryLoadValue(ar, "left", left);
        TryLoadValue(ar, "top", top);
        TryLoadValue(ar, "right", right);
        TryLoadValue(ar, "bottom", bottom);
    }
};

struct JsonOrigin
{
    int x = 0;
    int y = 0;
    template <class Archive>
    void load(Archive &ar)
    {
        TryLoadValue(ar, "x", x);
        TryLoadValue(ar, "y", y);
    }
};

struct JsonTexture
{
    std::string id;
    std::string file;
    int width = 0;
    int height = 0;
    template <class Archive>
    void load(Archive &ar)
    {
        TryLoadValue(ar, "id", id);
        TryLoadValue(ar, "file", file);
        TryLoadValue(ar, "width", width);
        TryLoadValue(ar, "height", height);
    }
};

struct JsonSprite
{
    std::string name;
    int id = 0;
    std::string texture;
    JsonBounds bounds;
    JsonOrigin origin;
    int maskSprite = 0;
    template <class Archive>
    void load(Archive &ar)
    {
        TryLoadValue(ar, "name", name);
        TryLoadValue(ar, "id", id);
        TryLoadValue(ar, "texture", texture);
        TryLoadValue(ar, "bounds", bounds);
        TryLoadValue(ar, "origin", origin);
        TryLoadValue(ar, "maskSprite", maskSprite);
    }
};

struct JsonGrid
{
    std::string texture;
    std::string prefix;
    int startId = 0;
    int rows = 0;
    int cols = 0;
    int cellW = 0;
    int cellH = 0;
    JsonOrigin origin;
    int maskGridStartId = 0;
    template <class Archive>
    void load(Archive &ar)
    {
        TryLoadValue(ar, "texture", texture);
        TryLoadValue(ar, "prefix", prefix);
        TryLoadValue(ar, "startId", startId);
        TryLoadValue(ar, "rows", rows);
        TryLoadValue(ar, "cols", cols);
        TryLoadValue(ar, "cellW", cellW);
        TryLoadValue(ar, "cellH", cellH);
        TryLoadValue(ar, "origin", origin);
        TryLoadValue(ar, "maskGridStartId", maskGridStartId);
    }
};

struct JsonAnimFrame
{
    int sprite = 0;
    int durationMs = 0;
    template <class Archive>
    void load(Archive &ar)
    {
        TryLoadValue(ar, "sprite", sprite);
        TryLoadValue(ar, "durationMs", durationMs);
    }
};

struct JsonAnim
{
    std::string name;
    int id = 0;
    bool loop = false;
    std::vector<JsonAnimFrame> frames;
    template <class Archive>
    void load(Archive &ar)
    {
        TryLoadValue(ar, "name", name);
        TryLoadValue(ar, "id", id);
        TryLoadValue(ar, "loop", loop);
        TryLoadValue(ar, "frames", frames);
    }
};

struct JsonSheet
{
    // Plain aggregate, deliberately NO load(): cereal's JSON archive enters
    // every struct processed through ar() via startNode(), so an unnamed
    // root struct would consume one iterator level too many and every member
    // lookup would miss. The four members are loaded directly off the
    // archive instead (see ParseJsonSheet).
    std::vector<JsonTexture> textures;
    std::vector<JsonSprite> sprites;
    std::vector<JsonGrid> grids;
    std::vector<JsonAnim> animations;
};

// Broad catch is the point: syntax errors, missing keys that some other
// layer requires, mistyped scalars (via the assert override above) — any
// failure means malformed input, and every caller maps false to reject.
bool ParseJsonSheet(const std::string &json, JsonSheet &out)
{
    try
    {
        std::istringstream in(json);
        cereal::JSONInputArchive ar(in);
        JsonSheet sheet;
        TryLoadValue(ar, "textures", sheet.textures);
        TryLoadValue(ar, "sprites", sheet.sprites);
        TryLoadValue(ar, "grids", sheet.grids);
        TryLoadValue(ar, "animations", sheet.animations);
        out = sheet;
        return true;
    }
    catch (const std::exception &)
    {
        return false;
    }
}

} // namespace

bool ValidateSpriteSheet(const JsonSheet &proto, SpriteSheetData &out);

bool ParseSpriteSheetJson(const std::string &json, SpriteSheetData &out)
{
    JsonSheet proto;
    if (!ParseJsonSheet(json, proto))
    {
        return false;
    }
    return ValidateSpriteSheet(proto, out);
}

bool ValidateSpriteSheet(const JsonSheet &proto, SpriteSheetData &out)
{
    SpriteSheetData sheet;

    // --- textures: unique non-empty ids, non-empty files, positive dims ---
    std::unordered_set<std::string> textureIds;
    for (const JsonTexture &t : proto.textures)
    {
        if (t.id.empty() || t.file.empty() || t.width <= 0 || t.height <= 0 ||
            !textureIds.insert(t.id).second)
        {
            return false;
        }
        SpriteTextureInfo info;
        info.id = t.id;
        info.file = t.file;
        info.width = t.width;
        info.height = t.height;
        sheet.textures.push_back(info);
    }

    auto findTexture = [&](const std::string &id) -> const SpriteTextureInfo * {
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

    auto pushSprite = [&](const std::string &name, int id, const std::string &texture,
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
    for (const JsonSprite &s : proto.sprites)
    {
        const SpriteRect bounds{ s.bounds.left, s.bounds.top, s.bounds.right, s.bounds.bottom };
        const SpriteOriginPx origin{ s.origin.x, s.origin.y };
        if (!pushSprite(s.name, s.id, s.texture, bounds, origin, s.maskSprite))
        {
            return false;
        }
    }

    // --- grids: expand to "<prefix>_<r>_<c>", ids start_id + r*cols + c ---
    for (const JsonGrid &g : proto.grids)
    {
        if (g.prefix.empty() || g.rows <= 0 || g.cols <= 0 || g.cellW <= 0 || g.cellH <= 0)
        {
            return false;
        }
        const SpriteTextureInfo *tex = findTexture(g.texture);
        if (tex == nullptr || g.cols * g.cellW > tex->width || g.rows * g.cellH > tex->height)
        {
            return false;
        }
        const SpriteOriginPx origin{ g.origin.x, g.origin.y };
        for (int r = 0; r < g.rows; ++r)
        {
            for (int c = 0; c < g.cols; ++c)
            {
                char name[128];
                std::snprintf(name, sizeof(name), "%s_%d_%d", g.prefix.c_str(), r, c);
                const SpriteRect bounds{ c * g.cellW, r * g.cellH, (c + 1) * g.cellW,
                                         (r + 1) * g.cellH };
                const int maskId =
                    g.maskGridStartId != 0 ? g.maskGridStartId + r * g.cols + c : 0;
                if (!pushSprite(name, g.startId + r * g.cols + c, g.texture, bounds, origin,
                                maskId))
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
    for (const JsonAnim &a : proto.animations)
    {
        if (a.name.empty() || !animIds.insert(a.id).second ||
            !animNames.insert(a.name).second || a.frames.empty())
        {
            return false;
        }
        SpriteAnimInfo info;
        info.name = a.name;
        info.id = a.id;
        info.loop = a.loop;
        for (const JsonAnimFrame &f : a.frames)
        {
            if (f.durationMs <= 0 || FindSpriteById(sheet, f.sprite) == nullptr)
            {
                return false;
            }
            SpriteAnimFrame frame;
            frame.sprite = f.sprite;
            frame.durationMs = f.durationMs;
            info.frames.push_back(frame);
        }
        sheet.animations.push_back(info);
    }

    out = sheet;
    return true;
}

bool LoadSpriteSheet(SpriteSheetData &out)
{
    // Atlas data lives split across data/configs/ (one SpriteSheet section
    // per file); the parts merge before the single validation pass, so
    // cross-file refs (animations -> grid sprites) resolve and duplicate
    // ids across files are still rejected. All-or-nothing: any missing or
    // malformed part fails the whole load, like the old single file did.
    static const char *kAtlasFiles[] = {
        "data/configs/textures.json",
        "data/configs/sprites.json",
        "data/configs/animations.json",
    };
    JsonSheet merged;
    for (const char *path : kAtlasFiles)
    {
        std::string json;
        if (!ReadWholeFile(path, json))
        {
            return false;
        }
        JsonSheet part;
        if (!ParseJsonSheet(json, part))
        {
            return false;
        }
        merged.textures.insert(merged.textures.end(), part.textures.begin(), part.textures.end());
        merged.sprites.insert(merged.sprites.end(), part.sprites.begin(), part.sprites.end());
        merged.grids.insert(merged.grids.end(), part.grids.begin(), part.grids.end());
        merged.animations.insert(merged.animations.end(), part.animations.begin(),
                                 part.animations.end());
    }
    return ValidateSpriteSheet(merged, out);
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
