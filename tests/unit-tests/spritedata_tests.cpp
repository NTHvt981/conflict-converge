// Unit tests for the cereal-JSON sprite-atlas loader (SpriteData).
// Covers the real data/configs atlas happy path plus every validation
// rejection via inline JSON (no temp files). Headless-safe: no textures.

#include "../unit-tests/test_harness.h"

#include "SpriteData.h"

namespace
{

const char *kValid = R"({
  "textures": [{ "id": "x", "file": "units/x.png", "width": 64, "height": 64 }],
  "sprites": [
    { "name": "hero", "id": 0, "texture": "x",
      "bounds": { "left": 0, "top": 0, "right": 32, "bottom": 32 },
      "origin": { "x": 16, "y": 16 } }
  ],
  "grids": [
    { "texture": "x", "prefix": "walk", "startId": 10,
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

    // --- malformed JSON fails cleanly (missing atlas files fail the
    // same way: LoadSpriteSheet is all-or-nothing over data/configs) ---
    {
        SpriteSheetData untouched;
        untouched.textures.push_back({ "sentinel", "sentinel.png", 1, 1 });
        SpriteSheetData out = untouched;
        CC_CHECK(!ParseSpriteSheetJson("{ not json", out));
        CC_CHECK(out.textures.size() == 1 && out.textures[0].id == "sentinel"); // untouched
    }

    // --- duplicate texture id (and empty ids) ---
    CC_CHECK(Rejects(R"({ "textures": [
      { "id": "a", "file": "a.png", "width": 32, "height": 32 },
      { "id": "a", "file": "b.png", "width": 32, "height": 32 } ] })"));
    CC_CHECK(Rejects(R"({ "textures": [
      { "id": "", "file": "a.png", "width": 32, "height": 32 } ] })"));

    // --- zero-size texture dims ---
    CC_CHECK(Rejects(R"({ "textures": [
      { "id": "a", "file": "a.png", "width": 0, "height": 32 } ] })"));

    // --- sprite referencing a missing texture ---
    CC_CHECK(Rejects(R"({ "textures": [
      { "id": "a", "file": "a.png", "width": 32, "height": 32 } ],
      "sprites": [{ "name": "s", "id": 0, "texture": "nope",
        "bounds": { "left": 0, "top": 0, "right": 8, "bottom": 8 } }] })"));

    // --- out-of-bounds and degenerate rects ---
    CC_CHECK(Rejects(R"({ "textures": [
      { "id": "a", "file": "a.png", "width": 32, "height": 32 } ],
      "sprites": [{ "name": "s", "id": 0, "texture": "a",
        "bounds": { "left": 0, "top": 0, "right": 64, "bottom": 64 } }] })"));
    CC_CHECK(Rejects(R"({ "textures": [
      { "id": "a", "file": "a.png", "width": 32, "height": 32 } ],
      "sprites": [{ "name": "s", "id": 0, "texture": "a",
        "bounds": { "left": 8, "top": 8, "right": 8, "bottom": 16 } }] })"));

    // --- duplicate sprite ids and names ---
    CC_CHECK(Rejects(R"({ "textures": [
      { "id": "a", "file": "a.png", "width": 32, "height": 32 } ],
      "sprites": [
        { "name": "a", "id": 0, "texture": "a",
          "bounds": { "left": 0, "top": 0, "right": 8, "bottom": 8 } },
        { "name": "b", "id": 0, "texture": "a",
          "bounds": { "left": 8, "top": 0, "right": 16, "bottom": 8 } } ] })"));
    CC_CHECK(Rejects(R"({ "textures": [
      { "id": "a", "file": "a.png", "width": 32, "height": 32 } ],
      "sprites": [
        { "name": "same", "id": 0, "texture": "a",
          "bounds": { "left": 0, "top": 0, "right": 8, "bottom": 8 } },
        { "name": "same", "id": 1, "texture": "a",
          "bounds": { "left": 8, "top": 0, "right": 16, "bottom": 8 } } ] })"));

    // --- grid overflowing its texture ---
    CC_CHECK(Rejects(R"({ "textures": [
      { "id": "a", "file": "a.png", "width": 32, "height": 32 } ],
      "grids": [{ "texture": "a", "prefix": "g", "startId": 0,
        "rows": 2, "cols": 2, "cellW": 32, "cellH": 32 }] })"));

    // --- grid colliding with an explicit sprite id ---
    CC_CHECK(Rejects(R"({ "textures": [
      { "id": "a", "file": "a.png", "width": 64, "height": 64 } ],
      "sprites": [{ "name": "s", "id": 1, "texture": "a",
        "bounds": { "left": 0, "top": 0, "right": 8, "bottom": 8 } }],
      "grids": [{ "texture": "a", "prefix": "g", "startId": 0,
        "rows": 1, "cols": 2, "cellW": 32, "cellH": 32 }] })"));

    // --- empty animation, bad duration, dangling sprite ref ---
    CC_CHECK(Rejects(R"({ "textures": [
      { "id": "a", "file": "a.png", "width": 32, "height": 32 } ],
      "animations": [{ "name": "a", "id": 0, "loop": true, "frames": [] }] })"));
    CC_CHECK(Rejects(R"({ "textures": [
      { "id": "a", "file": "a.png", "width": 32, "height": 32 } ],
      "sprites": [{ "name": "s", "id": 0, "texture": "a",
        "bounds": { "left": 0, "top": 0, "right": 8, "bottom": 8 } }],
      "animations": [{ "name": "a", "id": 0, "loop": true,
        "frames": [{ "sprite": 0, "durationMs": 0 }] }] })"));
    CC_CHECK(Rejects(R"({ "textures": [
      { "id": "a", "file": "a.png", "width": 32, "height": 32 } ],
      "animations": [{ "name": "a", "id": 0, "loop": true,
        "frames": [{ "sprite": 42, "durationMs": 100 }] }] })"));

    // --- dangling mask ref ---
    CC_CHECK(Rejects(R"({ "textures": [
      { "id": "a", "file": "a.png", "width": 32, "height": 32 } ],
      "sprites": [{ "name": "s", "id": 0, "texture": "a",
        "bounds": { "left": 0, "top": 0, "right": 8, "bottom": 8 },
        "maskSprite": 42 }] })"));

    // --- mask/base size mismatch ---
    CC_CHECK(Rejects(R"({ "textures": [
      { "id": "a", "file": "a.png", "width": 32, "height": 32 } ],
      "sprites": [
        { "name": "s", "id": 0, "texture": "a",
          "bounds": { "left": 0, "top": 0, "right": 8, "bottom": 8 },
          "maskSprite": 1 },
        { "name": "m", "id": 1, "texture": "a",
          "bounds": { "left": 0, "top": 0, "right": 16, "bottom": 16 } } ] })"));

    // --- valid mask pair + mask grid pairing ---
    {
        SpriteSheetData sheet;
        CC_CHECK(ParseSpriteSheetJson(R"({ "textures": [
          { "id": "base", "file": "base.png", "width": 64, "height": 32 },
          { "id": "mask", "file": "mask.png", "width": 64, "height": 32 } ],
          "sprites": [],
          "grids": [
            { "texture": "base", "prefix": "u", "startId": 0,
              "rows": 1, "cols": 2, "cellW": 32, "cellH": 32,
              "origin": { "x": 16, "y": 16 }, "maskGridStartId": 100 },
            { "texture": "mask", "prefix": "u_mask", "startId": 100,
              "rows": 1, "cols": 2, "cellW": 32, "cellH": 32,
              "origin": { "x": 16, "y": 16 } } ],
          "animations": [] })",
                                        sheet));
        const SpriteDefInfo *cell = FindSpriteByName(sheet, "u_0_1");
        CC_CHECK(cell != nullptr && cell->maskSprite == 101);
        const SpriteDefInfo *mask = FindSpriteById(sheet, 101);
        CC_CHECK(mask != nullptr && mask->name == "u_mask_0_1");
    }

    // --- real data/configs atlas files: 8 sheets, 152 sprites, 24 anims ---
    // Rifle-infantry renders from base+mask team-tint sheets (8 idle +
    // 8 dirs x 4 walk frames + 8 dirs x 2 attack frames, each with a mask);
    // prototype keeps its untinted sheets. PrototypeInfantry resolves its
    // own namespace, not rifle_infantry's.
    {
        SpriteSheetData sheet;
        CC_CHECK(LoadSpriteSheet(sheet));
        CC_CHECK(sheet.textures.size() == 8);
        CC_CHECK(sheet.sprites.size() == 152);
        CC_CHECK(sheet.animations.size() == 24);
        const SpriteDefInfo *first = FindSpriteByName(sheet, "rifle_infantry_idle_0_0");
        CC_CHECK(first != nullptr && first->id == 0 && first->texture == "rifle_idle_base");
        CC_CHECK(first->bounds.left == 0 && first->bounds.top == 0);
        CC_CHECK(first->bounds.right == 32 && first->bounds.bottom == 32);
        CC_CHECK(first->origin.x == 16 && first->origin.y == 16);
        CC_CHECK(first->maskSprite == 500); // team-tint mask grid
        const SpriteDefInfo *firstMask = FindSpriteById(sheet, 500);
        CC_CHECK(firstMask != nullptr && firstMask->name == "rifle_infantry_idle_mask_0_0" &&
                 firstMask->texture == "rifle_idle_mask" && firstMask->maskSprite == 0);
        // Walk grid is 8 direction columns x 4 frames; the wired anim uses
        // column 6 (right): ids 106/114/122/130.
        const SpriteDefInfo *last = FindSpriteByName(sheet, "rifle_infantry_walk_3_6");
        CC_CHECK(last != nullptr && last->id == 130 && last->texture == "rifle_run_base");
        CC_CHECK(last->bounds.left == 192 && last->bounds.top == 96);
        CC_CHECK(last->bounds.right == 224 && last->bounds.bottom == 128);
        CC_CHECK(last->maskSprite == 630);
        const SpriteAnimInfo *walk = FindAnimByName(sheet, "rifle_infantry_walk_6");
        CC_CHECK(walk != nullptr && walk->loop && walk->frames.size() == 4);
        CC_CHECK(walk->frames[0].sprite == 106 && walk->frames[3].sprite == 130);
        const SpriteAnimInfo *walkTop = FindAnimByName(sheet, "rifle_infantry_walk_0");
        CC_CHECK(walkTop != nullptr && walkTop->frames.size() == 4);
        CC_CHECK(walkTop->frames[0].sprite == 100 && walkTop->frames[3].sprite == 124);
        // Attack grid is 8 direction columns x 2 frames (column 6: 406/414).
        const SpriteDefInfo *strike = FindSpriteByName(sheet, "rifle_infantry_attack_1_6");
        CC_CHECK(strike != nullptr && strike->id == 414 &&
                 strike->texture == "rifle_attack_base");
        CC_CHECK(strike->bounds.left == 192 && strike->bounds.top == 32);
        CC_CHECK(strike->bounds.right == 224 && strike->bounds.bottom == 64);
        CC_CHECK(strike->maskSprite == 714);
        const SpriteAnimInfo *attack = FindAnimByName(sheet, "rifle_infantry_attack_6");
        CC_CHECK(attack != nullptr && attack->loop && attack->frames.size() == 2);
        CC_CHECK(attack->frames[0].sprite == 406 && attack->frames[1].sprite == 414);
        CC_CHECK(FindAnimByName(sheet, "rifle_infantry_walk") == nullptr); // split by direction
        CC_CHECK(FindAnimByName(sheet, "rifle_infantry_idle") == nullptr); // idle is sprite-direct
        CC_CHECK(FindAnimByName(sheet, "rifle_infantry_attack") == nullptr); // split by direction
        // Prototype namespace mirrors it (own textures, ids 200+/300+).
        const SpriteDefInfo *protoIdle = FindSpriteByName(sheet, "prototypeinfantry_idle_0_0");
        CC_CHECK(protoIdle != nullptr && protoIdle->id == 200 &&
                 protoIdle->texture == "prototype_infantry_idle");
        const SpriteDefInfo *protoWalk = FindSpriteByName(sheet, "prototypeinfantry_walk_3_6");
        CC_CHECK(protoWalk != nullptr && protoWalk->id == 330 &&
                 protoWalk->texture == "prototype_infantry_run");
        const SpriteAnimInfo *protoAnim = FindAnimByName(sheet, "prototypeinfantry_walk_6");
        CC_CHECK(protoAnim != nullptr && protoAnim->frames.size() == 4);
        CC_CHECK(protoAnim->frames[0].sprite == 306 && protoAnim->frames[3].sprite == 330);
    }
}
