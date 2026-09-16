// Unit tests for the protobuf-JSON sprite-atlas loader (SpriteData).
// Covers the real data/sprites.json happy path plus every validation
// rejection via inline JSON (no temp files). Headless-safe: no textures.

#include "../unit-tests/test_harness.h"

#include "SpriteData.h"

namespace
{

const char *kValid = R"({
  "textures": [{ "id": 0, "file": "units/x.png", "width": 64, "height": 64 }],
  "sprites": [
    { "name": "hero", "id": 0, "texture": 0,
      "bounds": { "left": 0, "top": 0, "right": 32, "bottom": 32 },
      "origin": { "x": 16, "y": 16 } }
  ],
  "grids": [
    { "texture": 0, "prefix": "walk", "startId": 10,
      "rows": 2, "cols": 2, "cellW": 32, "cellH": 32,
      "origin": { "x": 16, "y": 16 } }
  ],
  "animations": [
    { "name": "walk", "id": 0, "loop": true,
      "frames": [{ "sprite": 10, "durationMs": 100 },
                 { "sprite": 13, "durationMs": 100 }] }
  ]
})";

bool Parses(const char *json, SpriteSheetData &out)
{
    return ParseSpriteSheetJson(json, out);
}

bool Rejects(const char *json)
{
    SpriteSheetData out;
    return !ParseSpriteSheetJson(json, out);
}

} // namespace

void RunSpriteDataTests()
{
    // --- valid doc: explicit sprite + 2x2 grid + animation ---
    {
        SpriteSheetData sheet;
        CC_CHECK(Parses(kValid, sheet));
        CC_CHECK(sheet.textures.size() == 1);
        CC_CHECK(sheet.textures[0].file == "units/x.png");
        CC_CHECK(sheet.sprites.size() == 5); // 1 explicit + 4 grid
        const SpriteDefInfo *hero = FindSpriteByName(sheet, "hero");
        CC_CHECK(hero != nullptr && hero->id == 0);
        CC_CHECK(hero->bounds.right == 32 && hero->bounds.bottom == 32);
        CC_CHECK(hero->origin.x == 16 && hero->origin.y == 16);
        const SpriteDefInfo *cell = FindSpriteByName(sheet, "walk_1_1");
        CC_CHECK(cell != nullptr && cell->id == 13);
        CC_CHECK(cell->bounds.left == 32 && cell->bounds.top == 32);
        CC_CHECK(cell->bounds.right == 64 && cell->bounds.bottom == 64);
        CC_CHECK(FindSpriteById(sheet, 10) != nullptr);
        CC_CHECK(FindSpriteById(sheet, 11) != nullptr);
        CC_CHECK(FindSpriteById(sheet, 12) != nullptr);
        const SpriteAnimInfo *walk = FindAnimByName(sheet, "walk");
        CC_CHECK(walk != nullptr && walk->loop && walk->frames.size() == 2);
        CC_CHECK(walk->frames[0].sprite == 10 && walk->frames[0].durationMs == 100);
        CC_CHECK(FindAnimById(sheet, 0) == walk);
        CC_CHECK(FindSpriteByName(sheet, "missing") == nullptr);
        CC_CHECK(FindSpriteById(sheet, 999) == nullptr);
        CC_CHECK(FindAnimByName(sheet, "missing") == nullptr);
        CC_CHECK(FindAnimById(sheet, 999) == nullptr);
    }

    // --- malformed JSON and missing files fail cleanly ---
    {
        SpriteSheetData untouched;
        untouched.textures.push_back({ 7, "sentinel.png", 1, 1 });
        SpriteSheetData out = untouched;
        CC_CHECK(!ParseSpriteSheetJson("{ not json", out));
        CC_CHECK(out.textures.size() == 1 && out.textures[0].id == 7); // untouched
        CC_CHECK(!LoadSpriteSheet("data/does-not-exist.json", out));
        CC_CHECK(out.textures.size() == 1 && out.textures[0].id == 7); // untouched
    }

    // --- duplicate texture id ---
    CC_CHECK(Rejects(R"({ "textures": [
      { "id": 0, "file": "a.png", "width": 32, "height": 32 },
      { "id": 0, "file": "b.png", "width": 32, "height": 32 } ] })"));

    // --- zero-size texture dims ---
    CC_CHECK(Rejects(R"({ "textures": [
      { "id": 0, "file": "a.png", "width": 0, "height": 32 } ] })"));

    // --- sprite referencing a missing texture ---
    CC_CHECK(Rejects(R"({ "textures": [
      { "id": 0, "file": "a.png", "width": 32, "height": 32 } ],
      "sprites": [{ "name": "s", "id": 0, "texture": 9,
        "bounds": { "left": 0, "top": 0, "right": 8, "bottom": 8 } }] })"));

    // --- out-of-bounds and degenerate rects ---
    CC_CHECK(Rejects(R"({ "textures": [
      { "id": 0, "file": "a.png", "width": 32, "height": 32 } ],
      "sprites": [{ "name": "s", "id": 0, "texture": 0,
        "bounds": { "left": 0, "top": 0, "right": 64, "bottom": 64 } }] })"));
    CC_CHECK(Rejects(R"({ "textures": [
      { "id": 0, "file": "a.png", "width": 32, "height": 32 } ],
      "sprites": [{ "name": "s", "id": 0, "texture": 0,
        "bounds": { "left": 8, "top": 8, "right": 8, "bottom": 16 } }] })"));

    // --- duplicate sprite ids and names ---
    CC_CHECK(Rejects(R"({ "textures": [
      { "id": 0, "file": "a.png", "width": 32, "height": 32 } ],
      "sprites": [
        { "name": "a", "id": 0, "texture": 0,
          "bounds": { "left": 0, "top": 0, "right": 8, "bottom": 8 } },
        { "name": "b", "id": 0, "texture": 0,
          "bounds": { "left": 8, "top": 0, "right": 16, "bottom": 8 } } ] })"));
    CC_CHECK(Rejects(R"({ "textures": [
      { "id": 0, "file": "a.png", "width": 32, "height": 32 } ],
      "sprites": [
        { "name": "same", "id": 0, "texture": 0,
          "bounds": { "left": 0, "top": 0, "right": 8, "bottom": 8 } },
        { "name": "same", "id": 1, "texture": 0,
          "bounds": { "left": 8, "top": 0, "right": 16, "bottom": 8 } } ] })"));

    // --- grid overflowing its texture ---
    CC_CHECK(Rejects(R"({ "textures": [
      { "id": 0, "file": "a.png", "width": 32, "height": 32 } ],
      "grids": [{ "texture": 0, "prefix": "g", "startId": 0,
        "rows": 2, "cols": 2, "cellW": 32, "cellH": 32 }] })"));

    // --- grid colliding with an explicit sprite id ---
    CC_CHECK(Rejects(R"({ "textures": [
      { "id": 0, "file": "a.png", "width": 64, "height": 64 } ],
      "sprites": [{ "name": "s", "id": 1, "texture": 0,
        "bounds": { "left": 0, "top": 0, "right": 8, "bottom": 8 } }],
      "grids": [{ "texture": 0, "prefix": "g", "startId": 0,
        "rows": 1, "cols": 2, "cellW": 32, "cellH": 32 }] })"));

    // --- empty animation, bad duration, dangling sprite ref ---
    CC_CHECK(Rejects(R"({ "textures": [
      { "id": 0, "file": "a.png", "width": 32, "height": 32 } ],
      "animations": [{ "name": "a", "id": 0, "loop": true, "frames": [] }] })"));
    CC_CHECK(Rejects(R"({ "textures": [
      { "id": 0, "file": "a.png", "width": 32, "height": 32 } ],
      "sprites": [{ "name": "s", "id": 0, "texture": 0,
        "bounds": { "left": 0, "top": 0, "right": 8, "bottom": 8 } }],
      "animations": [{ "name": "a", "id": 0, "loop": true,
        "frames": [{ "sprite": 0, "durationMs": 0 }] }] })"));
    CC_CHECK(Rejects(R"({ "textures": [
      { "id": 0, "file": "a.png", "width": 32, "height": 32 } ],
      "animations": [{ "name": "a", "id": 0, "loop": true,
        "frames": [{ "sprite": 42, "durationMs": 100 }] }] })"));

    // --- dangling mask ref ---
    CC_CHECK(Rejects(R"({ "textures": [
      { "id": 0, "file": "a.png", "width": 32, "height": 32 } ],
      "sprites": [{ "name": "s", "id": 0, "texture": 0,
        "bounds": { "left": 0, "top": 0, "right": 8, "bottom": 8 },
        "maskSprite": 42 }] })"));

    // --- mask/base size mismatch ---
    CC_CHECK(Rejects(R"({ "textures": [
      { "id": 0, "file": "a.png", "width": 32, "height": 32 } ],
      "sprites": [
        { "name": "s", "id": 0, "texture": 0,
          "bounds": { "left": 0, "top": 0, "right": 8, "bottom": 8 },
          "maskSprite": 1 },
        { "name": "m", "id": 1, "texture": 0,
          "bounds": { "left": 0, "top": 0, "right": 16, "bottom": 16 } } ] })"));

    // --- valid mask pair + mask grid pairing ---
    {
        SpriteSheetData sheet;
        CC_CHECK(ParseSpriteSheetJson(R"({ "textures": [
          { "id": 0, "file": "base.png", "width": 64, "height": 32 },
          { "id": 1, "file": "mask.png", "width": 64, "height": 32 } ],
          "sprites": [],
          "grids": [
            { "texture": 0, "prefix": "u", "startId": 0,
              "rows": 1, "cols": 2, "cellW": 32, "cellH": 32,
              "origin": { "x": 16, "y": 16 }, "maskGridStartId": 100 },
            { "texture": 1, "prefix": "u_mask", "startId": 100,
              "rows": 1, "cols": 2, "cellW": 32, "cellH": 32,
              "origin": { "x": 16, "y": 16 } } ],
          "animations": [] })",
                                        sheet));
        const SpriteDefInfo *cell = FindSpriteByName(sheet, "u_0_1");
        CC_CHECK(cell != nullptr && cell->maskSprite == 101);
        const SpriteDefInfo *mask = FindSpriteById(sheet, 101);
        CC_CHECK(mask != nullptr && mask->name == "u_mask_0_1");
    }

    // --- real data/sprites.json: 4 sheets, 256 grid sprites, 2 anims ---
    {
        SpriteSheetData sheet;
        CC_CHECK(LoadSpriteSheet("data/sprites.json", sheet));
        CC_CHECK(sheet.textures.size() == 4);
        CC_CHECK(sheet.sprites.size() == 256);
        const SpriteDefInfo *first = FindSpriteByName(sheet, "infantry_idle_0_0");
        CC_CHECK(first != nullptr && first->id == 0 && first->texture == 0);
        CC_CHECK(first->bounds.left == 0 && first->bounds.top == 0);
        CC_CHECK(first->bounds.right == 32 && first->bounds.bottom == 32);
        CC_CHECK(first->maskSprite == 1000); // base+mask paired via grids
        const SpriteDefInfo *firstMask = FindSpriteById(sheet, 1000);
        CC_CHECK(firstMask != nullptr && firstMask->name == "infantry_idle_mask_0_0");
        CC_CHECK(firstMask->bounds.right - firstMask->bounds.left == 32);
        const SpriteDefInfo *last = FindSpriteByName(sheet, "infantry_walk_7_7");
        CC_CHECK(last != nullptr && last->id == 2063 && last->texture == 2);
        CC_CHECK(last->bounds.left == 224 && last->bounds.top == 224);
        CC_CHECK(last->bounds.right == 256 && last->bounds.bottom == 256);
        const SpriteAnimInfo *walk = FindAnimByName(sheet, "infantry_walk");
        CC_CHECK(walk != nullptr && walk->loop && walk->frames.size() == 4);
    }
}
