#include "app/SpriteData.h"

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
    std::vector<JsonTexture> textures;
    std::vector<JsonSprite> sprites;
    std::vector<JsonGrid> grids;
    std::vector<JsonAnim> animations;
};

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

}

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
        info.maskSprite = maskSprite;
        sheet.sprites.push_back(info);
        return true;
    };

    for (const JsonSprite &s : proto.sprites)
    {
        const SpriteRect bounds{ s.bounds.left, s.bounds.top, s.bounds.right, s.bounds.bottom };
        const SpriteOriginPx origin{ s.origin.x, s.origin.y };
        if (!pushSprite(s.name, s.id, s.texture, bounds, origin, s.maskSprite))
        {
            return false;
        }
    }

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

    for (const SpriteDefInfo &s : sheet.sprites)
    {
        if (s.maskSprite == 0)
        {
            continue;
        }
        const SpriteDefInfo *mask = FindSpriteById(sheet, s.maskSprite);
        if (mask == nullptr)
        {
            return false;
        }
        const int w = s.bounds.right - s.bounds.left;
        const int h = s.bounds.bottom - s.bounds.top;
        const int mw = mask->bounds.right - mask->bounds.left;
        const int mh = mask->bounds.bottom - mask->bounds.top;
        if (w != mw || h != mh)
        {
            return false;
        }
    }

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
