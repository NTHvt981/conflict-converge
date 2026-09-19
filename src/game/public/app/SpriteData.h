#pragma once

#include <string>
#include <vector>

// Sprite-atlas data (JSON in data/configs/: textures.json, sprites.json,
// animations.json — one section each, merged at load; parsed via cereal-JSON).

struct SpriteRect
{
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
};

struct SpriteOriginPx
{
    int x = 0;
    int y = 0;
};

struct SpriteTextureInfo
{
    std::string id;   // human-readable key, e.g. "infantry_idle_base"
    std::string file; // relative to data/sprites/
    int width = 0;
    int height = 0;
};

struct SpriteDefInfo
{
    std::string name;
    int id = 0;
    std::string texture; // SpriteTextureInfo.id
    SpriteRect bounds;
    SpriteOriginPx origin;
    // Team-color mask: SpriteDefInfo.id of the paired mask sprite; 0 = no
    // mask (whole sprite tinted).
    int maskSprite = 0;
};

struct SpriteAnimFrame
{
    int sprite = 0; // SpriteDefInfo.id
    int durationMs = 0;
};

struct SpriteAnimInfo
{
    std::string name;
    int id = 0;
    bool loop = false;
    std::vector<SpriteAnimFrame> frames;
};

struct SpriteSheetData
{
    std::vector<SpriteTextureInfo> textures;
    std::vector<SpriteDefInfo> sprites; // explicit + grid-expanded, load order
    std::vector<SpriteAnimInfo> animations;
};

// Load the three atlas files merged as one sheet. False when any file is
// missing/unreadable/malformed or the merged sheet fails validation; `out`
// untouched then.
bool LoadSpriteSheet(SpriteSheetData &out);

// Parse sprite JSON from memory (same validation as LoadSpriteSheet).
bool ParseSpriteSheetJson(const std::string &json, SpriteSheetData &out);

// Lookups by name/id. Null when absent.
const SpriteDefInfo *FindSpriteByName(const SpriteSheetData &sheet, const std::string &name);
const SpriteDefInfo *FindSpriteById(const SpriteSheetData &sheet, int id);
const SpriteAnimInfo *FindAnimByName(const SpriteSheetData &sheet, const std::string &name);
const SpriteAnimInfo *FindAnimById(const SpriteSheetData &sheet, int id);
