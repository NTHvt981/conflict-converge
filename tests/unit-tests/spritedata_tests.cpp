// Unit tests for the cereal-JSON sprite-atlas loader (SpriteData).
// Covers the real data/configs atlas happy path plus every validation
// rejection via inline JSON (no temp files). Headless-safe: no textures.

#include "../unit-tests/test_harness.h"

#include "app/data/SpriteData.h"

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
    { "texture": "x", "prefix": "move", "startId": 10,
      "rows": 2, "cols": 2, "cellW": 32, "cellH": 32,
      "origin": { "x": 16, "y": 16 } }
  ],
  "animations": [
    { "name": "move", "id": 0, "loop": true,
      "frames": [{ "sprite": 10, "durationMs": 100 },
                 { "sprite": 13, "durationMs": 100 }] }
  ]
})";

bool Parses(const char *json, SpriteSheetData &out)
{
	const std::expected<SpriteSheetData, ConfigError> parsed = ParseSpriteSheetJson(json);
	out = parsed.value_or(SpriteSheetData{});
	return parsed.has_value();
}

bool Rejects(const char *json)
{
	return !ParseSpriteSheetJson(json).has_value();
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
		const SpriteDefInfo *cell = FindSpriteByName(sheet, "move_1_1");
		CC_CHECK(cell != nullptr && cell->id == 13);
		CC_CHECK(cell->bounds.left == 32 && cell->bounds.top == 32);
		CC_CHECK(cell->bounds.right == 64 && cell->bounds.bottom == 64);
		CC_CHECK(FindSpriteById(sheet, 10) != nullptr);
		CC_CHECK(FindSpriteById(sheet, 11) != nullptr);
		CC_CHECK(FindSpriteById(sheet, 12) != nullptr);
		const SpriteAnimInfo *move = FindAnimByName(sheet, "move");
		CC_CHECK(move != nullptr && move->loop && move->frames.size() == 2);
		CC_CHECK(move->frames[0].sprite == 10 && move->frames[0].durationMs == 100);
		CC_CHECK(FindAnimById(sheet, 0) == move);
		CC_CHECK(FindSpriteByName(sheet, "missing") == nullptr);
		CC_CHECK(FindSpriteById(sheet, 999) == nullptr);
		CC_CHECK(FindAnimByName(sheet, "missing") == nullptr);
		CC_CHECK(FindAnimById(sheet, 999) == nullptr);
	}

	// --- malformed JSON yields no sheet (missing atlas files fail the
	// same way: LoadSpriteSheet is all-or-nothing over data/configs) ---
	{
		const std::expected<SpriteSheetData, ConfigError> malformed =
			ParseSpriteSheetJson("{ not json");
		CC_CHECK(!malformed.has_value() &&
				 malformed.error() == ConfigError::MalformedJson);
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
		const std::expected<SpriteSheetData, ConfigError> parsed = ParseSpriteSheetJson(R"({ "textures": [
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
		  "animations": [] })");
		CC_CHECK(parsed.has_value());
		const SpriteSheetData sheet = parsed.value_or(SpriteSheetData{});
		const SpriteDefInfo *cell = FindSpriteByName(sheet, "u_0_1");
		CC_CHECK(cell != nullptr && cell->maskSprite == 101);
		const SpriteDefInfo *mask = FindSpriteById(sheet, 101);
		CC_CHECK(mask != nullptr && mask->name == "u_mask_0_1");
	}

	// --- real data/configs atlas files: 38 sheets, 1608 sprites, 136 anims ---
	// Rifle-infantry renders from base+mask team-tint sheets on 72x72 cells
	// (8 rows x 8 direction columns per action: idle + move + 4-frame
	// attack + 4-frame death, each with a mask); antiarmor mirrors it on
	// the same 72x72 cells; engineer and medic add the same 4-action set;
	// lighttank adds body + head 8-column stacks (each with a mask);
	// prototype keeps its untinted sheets.
	// PrototypeInfantry resolves its own namespace, not rifle_infantry's.
	{
		const std::expected<SpriteSheetData, ConfigError> loaded = LoadSpriteSheet();
		CC_CHECK(loaded.has_value());
		const SpriteSheetData sheet = loaded.value_or(SpriteSheetData{});
		CC_CHECK(sheet.textures.size() == 38);
		CC_CHECK(sheet.sprites.size() == 1608);
		CC_CHECK(sheet.animations.size() == 136);
		const SpriteDefInfo *first = FindSpriteByName(sheet, "rifle_infantry_idle_0_0");
		CC_CHECK(first != nullptr && first->id == 2000 && first->texture == "rifle_infantry_idle_base");
		CC_CHECK(first->bounds.left == 0 && first->bounds.top == 0);
		CC_CHECK(first->bounds.right == 72 && first->bounds.bottom == 72);
		CC_CHECK(first->origin.x == 36 && first->origin.y == 36);
		CC_CHECK(first->maskSprite == 2100); // team-tint mask grid
		const SpriteDefInfo *firstMask = FindSpriteById(sheet, 2100);
		CC_CHECK(firstMask != nullptr && firstMask->name == "rifle_infantry_idle_mask_0_0" &&
				 firstMask->texture == "rifle_infantry_idle_mask" && firstMask->maskSprite == 0);
		// Move grid is 8 direction columns x 8 frame rows; the wired anim
		// uses column 6 (right): ids 2206..2262.
		const SpriteDefInfo *last = FindSpriteByName(sheet, "rifle_infantry_move_3_6");
		CC_CHECK(last != nullptr && last->id == 2230 && last->texture == "rifle_infantry_move_base");
		CC_CHECK(last->bounds.left == 432 && last->bounds.top == 216);
		CC_CHECK(last->bounds.right == 504 && last->bounds.bottom == 288);
		CC_CHECK(last->maskSprite == 2330);
		const SpriteAnimInfo *move = FindAnimByName(sheet, "rifle_infantry_move_6");
		CC_CHECK(move != nullptr && move->loop && move->frames.size() == 8);
		CC_CHECK(move->frames[0].sprite == 2206 && move->frames[7].sprite == 2262);
		CC_CHECK(move->frames[0].durationMs == 250);
		const SpriteAnimInfo *moveTop = FindAnimByName(sheet, "rifle_infantry_move_0");
		CC_CHECK(moveTop != nullptr && moveTop->frames.size() == 8);
		CC_CHECK(moveTop->frames[0].sprite == 2200 && moveTop->frames[7].sprite == 2256);
		// Attack grid is 8 direction columns x 4 frames (column 6:
		// 2406/2414/2422/2430).
		const SpriteDefInfo *strike = FindSpriteByName(sheet, "rifle_infantry_attack_1_6");
		CC_CHECK(strike != nullptr && strike->id == 2414 &&
				 strike->texture == "rifle_infantry_attack_base");
		CC_CHECK(strike->bounds.left == 432 && strike->bounds.top == 72);
		CC_CHECK(strike->bounds.right == 504 && strike->bounds.bottom == 144);
		CC_CHECK(strike->maskSprite == 2514);
		const SpriteAnimInfo *attack = FindAnimByName(sheet, "rifle_infantry_attack_6");
		CC_CHECK(attack != nullptr && attack->loop && attack->frames.size() == 4);
		CC_CHECK(attack->frames[0].sprite == 2406 && attack->frames[3].sprite == 2430);
		CC_CHECK(attack->frames[0].durationMs == 250);
		// Death grid is 8 direction columns x 4 frames; its per-direction
		// anims play once (no loop) so the last frame holds.
		const SpriteDefInfo *deathCell = FindSpriteByName(sheet, "rifle_infantry_death_2_6");
		CC_CHECK(deathCell != nullptr && deathCell->id == 2622 &&
				 deathCell->texture == "rifle_infantry_death_base");
		const SpriteAnimInfo *death = FindAnimByName(sheet, "rifle_infantry_death_6");
		CC_CHECK(death != nullptr && !death->loop && death->frames.size() == 4);
		CC_CHECK(death->frames[0].sprite == 2606 && death->frames[3].sprite == 2630);
		CC_CHECK(FindAnimByName(sheet, "rifle_infantry_move") == nullptr); // split by direction
		CC_CHECK(FindAnimByName(sheet, "rifle_infantry_idle") == nullptr); // split by direction
		CC_CHECK(FindAnimByName(sheet, "rifle_infantry_attack") == nullptr); // split by direction
		// Antiarmor namespace on 72x72 cells (idle 2800+/move 3000+/attack
		// 3200+/death 3400+; move column 6: 3006..3062).
		const SpriteDefInfo *aaIdle = FindSpriteByName(sheet, "antiarmor_idle_0_0");
		CC_CHECK(aaIdle != nullptr && aaIdle->id == 2800 &&
				 aaIdle->texture == "antiarmor_idle_base");
		CC_CHECK(aaIdle->bounds.left == 0 && aaIdle->bounds.top == 0);
		CC_CHECK(aaIdle->bounds.right == 72 && aaIdle->bounds.bottom == 72);
		CC_CHECK(aaIdle->origin.x == 36 && aaIdle->origin.y == 36);
		CC_CHECK(aaIdle->maskSprite == 2900);
		const SpriteDefInfo *aaMove = FindSpriteByName(sheet, "antiarmor_move_3_6");
		CC_CHECK(aaMove != nullptr && aaMove->id == 3030 &&
				 aaMove->texture == "antiarmor_move_base");
		CC_CHECK(aaMove->bounds.left == 432 && aaMove->bounds.top == 216);
		CC_CHECK(aaMove->bounds.right == 504 && aaMove->bounds.bottom == 288);
		CC_CHECK(aaMove->maskSprite == 3130);
		const SpriteAnimInfo *aaMoveAnim = FindAnimByName(sheet, "antiarmor_move_6");
		CC_CHECK(aaMoveAnim != nullptr && aaMoveAnim->loop && aaMoveAnim->frames.size() == 8);
		CC_CHECK(aaMoveAnim->frames[0].sprite == 3006 && aaMoveAnim->frames[7].sprite == 3062);
		// Attack grid is 8 direction columns x 4 frames (column 6:
		// 3206/3214/3222/3230).
		const SpriteDefInfo *aaStrike = FindSpriteByName(sheet, "antiarmor_attack_2_6");
		CC_CHECK(aaStrike != nullptr && aaStrike->id == 3222 &&
				 aaStrike->texture == "antiarmor_attack_base");
		CC_CHECK(aaStrike->bounds.left == 432 && aaStrike->bounds.top == 144);
		CC_CHECK(aaStrike->bounds.right == 504 && aaStrike->bounds.bottom == 216);
		CC_CHECK(aaStrike->maskSprite == 3322);
		const SpriteAnimInfo *aaAttack = FindAnimByName(sheet, "antiarmor_attack_6");
		CC_CHECK(aaAttack != nullptr && aaAttack->loop && aaAttack->frames.size() == 4);
		CC_CHECK(aaAttack->frames[0].sprite == 3206 && aaAttack->frames[3].sprite == 3230);
		// Engineer namespace on 72x72 cells (idle 3600+/move 3800+/attack
		// 4000+/death 4200+; move column 6 across 8 frames: 3806..3862).
		const SpriteDefInfo *engIdle = FindSpriteByName(sheet, "engineer_idle_0_0");
		CC_CHECK(engIdle != nullptr && engIdle->id == 3600 &&
				 engIdle->texture == "engineer_idle_base");
		CC_CHECK(engIdle->bounds.left == 0 && engIdle->bounds.top == 0);
		CC_CHECK(engIdle->bounds.right == 72 && engIdle->bounds.bottom == 72);
		CC_CHECK(engIdle->origin.x == 36 && engIdle->origin.y == 36);
		CC_CHECK(engIdle->maskSprite == 3700);
		const SpriteDefInfo *engMove = FindSpriteByName(sheet, "engineer_move_5_6");
		CC_CHECK(engMove != nullptr && engMove->id == 3846 &&
				 engMove->texture == "engineer_move_base");
		CC_CHECK(engMove->bounds.left == 432 && engMove->bounds.top == 360);
		CC_CHECK(engMove->bounds.right == 504 && engMove->bounds.bottom == 432);
		CC_CHECK(engMove->maskSprite == 3946);
		const SpriteAnimInfo *engMoveAnim = FindAnimByName(sheet, "engineer_move_6");
		CC_CHECK(engMoveAnim != nullptr && engMoveAnim->loop &&
				 engMoveAnim->frames.size() == 8);
		CC_CHECK(engMoveAnim->frames[0].sprite == 3806 &&
				 engMoveAnim->frames[7].sprite == 3862);
		// Engineer has its own 4-frame attack and death anims per direction.
		const SpriteAnimInfo *engAttack = FindAnimByName(sheet, "engineer_attack_6");
		CC_CHECK(engAttack != nullptr && engAttack->loop && engAttack->frames.size() == 4);
		CC_CHECK(engAttack->frames[0].sprite == 4006 && engAttack->frames[3].sprite == 4030);
		const SpriteAnimInfo *engDeath = FindAnimByName(sheet, "engineer_death_6");
		CC_CHECK(engDeath != nullptr && !engDeath->loop && engDeath->frames.size() == 4);
		CC_CHECK(engDeath->frames[0].sprite == 4206 && engDeath->frames[3].sprite == 4230);
		// Medic namespace on 72x72 cells (idle 4400+/move 4600+/attack
		// 4800+/death 5000+; move column 6 across 8 frames: 4606..4662).
		const SpriteDefInfo *medIdle = FindSpriteByName(sheet, "medic_idle_0_0");
		CC_CHECK(medIdle != nullptr && medIdle->id == 4400 &&
				 medIdle->texture == "medic_idle_base");
		CC_CHECK(medIdle->bounds.left == 0 && medIdle->bounds.top == 0);
		CC_CHECK(medIdle->bounds.right == 72 && medIdle->bounds.bottom == 72);
		CC_CHECK(medIdle->origin.x == 36 && medIdle->origin.y == 36);
		CC_CHECK(medIdle->maskSprite == 4500);
		const SpriteDefInfo *medMove = FindSpriteByName(sheet, "medic_move_3_6");
		CC_CHECK(medMove != nullptr && medMove->id == 4630 &&
				 medMove->texture == "medic_move_base");
		CC_CHECK(medMove->bounds.left == 432 && medMove->bounds.top == 216);
		CC_CHECK(medMove->bounds.right == 504 && medMove->bounds.bottom == 288);
		CC_CHECK(medMove->maskSprite == 4730);
		const SpriteAnimInfo *medMoveAnim = FindAnimByName(sheet, "medic_move_6");
		CC_CHECK(medMoveAnim != nullptr && medMoveAnim->loop &&
				 medMoveAnim->frames.size() == 8);
		CC_CHECK(medMoveAnim->frames[0].sprite == 4606 &&
				 medMoveAnim->frames[7].sprite == 4662);
		// Medic has its own 4-frame attack and death anims per direction.
		const SpriteAnimInfo *medAttack = FindAnimByName(sheet, "medic_attack_6");
		CC_CHECK(medAttack != nullptr && medAttack->loop && medAttack->frames.size() == 4);
		CC_CHECK(medAttack->frames[0].sprite == 4806 && medAttack->frames[3].sprite == 4830);
		const SpriteAnimInfo *medDeath = FindAnimByName(sheet, "medic_death_6");
		CC_CHECK(medDeath != nullptr && !medDeath->loop && medDeath->frames.size() == 4);
		CC_CHECK(medDeath->frames[0].sprite == 5006 && medDeath->frames[3].sprite == 5030);
		// Prototype namespace mirrors it (own textures, ids 200+/300+).
		const SpriteDefInfo *protoIdle = FindSpriteByName(sheet, "prototypeinfantry_idle_0_0");
		CC_CHECK(protoIdle != nullptr && protoIdle->id == 200 &&
				 protoIdle->texture == "prototype_infantry_idle");
		const SpriteDefInfo *protoMove = FindSpriteByName(sheet, "prototypeinfantry_move_3_6");
		CC_CHECK(protoMove != nullptr && protoMove->id == 330 &&
				 protoMove->texture == "prototype_infantry_move");
		const SpriteAnimInfo *protoAnim = FindAnimByName(sheet, "prototypeinfantry_move_6");
		CC_CHECK(protoAnim != nullptr && protoAnim->frames.size() == 4);
		CC_CHECK(protoAnim->frames[0].sprite == 306 && protoAnim->frames[3].sprite == 330);
	}
}
