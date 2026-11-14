// Unit tests for art (phase→frame mapping, rectangle fallback, atlas,
// squad slots, terrain tiles). Nothing here needs a GPU or audio device:
// Art::Init(InitMode::Headless) loads nothing and draw calls are never made headless.

#include "test_harness.h"

#include "app/data/Art.h"
#include "world/TileMap.h" // TerrainType for the terrain tile mapping

void RunArtTests()
{
	// --- attack phases map to sprite frames ---
	CC_CHECK(FrameForPhase(AttackPhase::Ready) == UnitFrame::Idle);
	CC_CHECK(FrameForPhase(AttackPhase::WindUp) == UnitFrame::Attack);
	CC_CHECK(FrameForPhase(AttackPhase::Recover) == UnitFrame::Attack);

	// --- headless art stays on rectangles ---
	{
		Art art;
		CC_CHECK(!art.Init(InitMode::Headless));
		CC_CHECK(!art.IsReady());
		CC_CHECK(art.UseRectangles());
		CC_CHECK(!art.HasFont()); // UI typeface loads on the device path only
		art.Shutdown(); // safe without Init(InitMode::WithDevice)
		CC_CHECK(art.UseRectangles());
		CC_CHECK(!art.HasFont());
	}

	// --- : SquadSlots ---
	{
		// RifleInfantry at full health → 5 soldiers
		const SquadLayout inf = SquadSlots(UnitType::RifleInfantry, 42, 1.0f);
		CC_CHECK(inf.count == 5);
		CC_CHECK(inf.scale < 1.0f); // clusters shrink: 5-wide at 0.55
		// All 5 offsets must be non-identical (spaced apart)
		CC_CHECK(inf.slots[0].x != inf.slots[3].x || inf.slots[0].y != inf.slots[3].y);

		// RifleInfantry at 40% health → 2 soldiers (0.40 is not > 0.40)
		const SquadLayout mid = SquadSlots(UnitType::RifleInfantry, 42, 0.40f);
		CC_CHECK(mid.count == 2);
		CC_CHECK(mid.scale > inf.scale); // fewer soldiers → larger blit

		// RifleInfantry at 5% health → 1 soldier at full scale
		const SquadLayout low = SquadSlots(UnitType::RifleInfantry, 42, 0.05f);
		CC_CHECK(low.count == 1);
		CC_CHECK(low.scale == 1.0f);

		// AntiArmorInfantry: 2-man team above half health, 1 below.
		const SquadLayout aa = SquadSlots(UnitType::AntiArmorInfantry, 10, 0.90f);
		CC_CHECK(aa.count == 2);
		CC_CHECK(aa.scale == 0.80f);
		CC_CHECK(SquadSlots(UnitType::AntiArmorInfantry, 10, 0.40f).count == 1);

		// Medic: 2-man team above half health, 1 below (like AntiArmor).
		const SquadLayout medic = SquadSlots(UnitType::Medic, 11, 0.90f);
		CC_CHECK(medic.count == 2);
		CC_CHECK(medic.scale == 0.80f);
		CC_CHECK(SquadSlots(UnitType::Medic, 11, 0.40f).count == 1);

		// Engineer: always a single centered sprite, like a vehicle.
		const SquadLayout engineer = SquadSlots(UnitType::Engineer, 20, 1.0f);
		CC_CHECK(engineer.count == 1);
		CC_CHECK(engineer.slots[0].x == 0.0f && engineer.slots[0].y == 0.0f);
		CC_CHECK(engineer.scale == 1.0f);

		// PrototypeInfantry clusters exactly like RifleInfantry (5 at full health).
		const SquadLayout prototype = SquadSlots(UnitType::PrototypeInfantry, 10, 1.0f);
		CC_CHECK(prototype.count == 5);
		CC_CHECK(prototype.scale == 0.55f);
		CC_CHECK(SquadSlots(UnitType::PrototypeInfantry, 10, 0.10f).count == 1);

		// Non-infantry always returns 1 with offset {0,0}
		const SquadLayout tank = SquadSlots(UnitType::HeavyTank, 99, 1.0f);
		CC_CHECK(tank.count == 1);
		CC_CHECK(tank.slots[0].x == 0.0f);
		CC_CHECK(tank.slots[0].y == 0.0f);
		CC_CHECK(tank.scale == 1.0f);

		// Deterministic: same id + health → same offsets
		const SquadLayout a = SquadSlots(UnitType::RifleInfantry, 77, 0.90f);
		const SquadLayout b = SquadSlots(UnitType::RifleInfantry, 77, 0.90f);
		CC_CHECK(a.slots[0].x == b.slots[0].x);
		CC_CHECK(a.slots[0].y == b.slots[0].y);
		CC_CHECK(a.scale == b.scale);

		// No overlap: every pair of active slots in a full-strength
		// RifleInfantry squad stays >= scale * 32px apart (jitter included).
		const SquadLayout full = SquadSlots(UnitType::RifleInfantry, 1234, 1.0f);
		CC_CHECK(full.count == 5);
		for (int i = 0; i < full.count; ++i)
		{
			for (int j = i + 1; j < full.count; ++j)
			{
				const float dx = full.slots[i].x - full.slots[j].x;
				const float dy = full.slots[i].y - full.slots[j].y;
				CC_CHECK(dx * dx + dy * dy >= full.scale * 32.0f * full.scale * 32.0f);
			}
		}
	}

	// --- atlas: down headless, draw is a safe no-op ---
	{
		Art art;
		CC_CHECK(!art.UseAtlas());
		CC_CHECK(art.UnitDeathAnimSeconds(UnitType::RifleInfantry) == 0.0f);
		CC_CHECK(art.UnitSprite(UnitType::RifleInfantry, false, 1, 0.0f, 6).empty());
		CC_CHECK(art.UnitDeathSprite(UnitType::RifleInfantry, 1, 0.0f, 6).empty());
		art.DrawAtlasFrame("rifle_infantry_idle_0_0", { 0.0f, 0.0f }, WHITE); // no crash
		// Default mode: raw BLUE/RED (colorBlindMode_ defaults to false).
		const Color blue = art.TeamTint(kPlayerTeam);
		CC_CHECK(blue.r == BLUE.r && blue.g == BLUE.g && blue.b == BLUE.b);
		const Color red = art.TeamTint(kEnemyTeam);
		CC_CHECK(red.r == RED.r && red.g == RED.g && red.b == RED.b);
		// Color-blind mode: Okabe-Ito orange / sky blue.
		art.SetColorBlindMode(true);
		const Color cbOrange = art.TeamTint(kPlayerTeam);
		CC_CHECK(cbOrange.r == 230 && cbOrange.g == 159 && cbOrange.b == 0);
		const Color cbSky = art.TeamTint(kEnemyTeam);
		CC_CHECK(cbSky.r == 86 && cbSky.g == 180 && cbSky.b == 233);
		art.SetColorBlindMode(false);
		const Color backToBlue = art.TeamTint(kPlayerTeam);
		CC_CHECK(backToBlue.r == BLUE.r && backToBlue.g == BLUE.g && backToBlue.b == BLUE.b);
	}

	// --- atlas: idle sprite + move cycle from the data/configs atlas ---
	{
		Art art;
		CC_CHECK(art.LoadAtlas().has_value());
		CC_CHECK(!art.UseAtlas()); // parsed, but no GPU textures headless
		// Idle anim is 8x250ms (4 fps) per direction column (column 0 = top);
		// id 1 offsets 37ms in: t=0 -> frame 0.
		CC_CHECK(art.UnitSprite(UnitType::RifleInfantry, false, 1, 0.0f, 0) == "rifle_infantry_idle_0_0");
		CC_CHECK(art.UnitSprite(UnitType::RifleInfantry, false, 1, 0.0f, 6) == "rifle_infantry_idle_0_6");
		CC_CHECK(art.UnitSprite(UnitType::RifleInfantry, false, 1, 0.0f, 3) == "rifle_infantry_idle_0_3");
		// Move anim is 8x250ms (4 fps) per direction column (column 6 = right);
		// id 1 offsets 37ms in: t=0 -> frame 0.
		CC_CHECK(art.UnitSprite(UnitType::RifleInfantry, true, 1, 0.0f, 6) == "rifle_infantry_move_0_6");
		// t=800ms + 37 offset = 837 -> frame 3.
		CC_CHECK(art.UnitSprite(UnitType::RifleInfantry, true, 1, 0.8f, 6) == "rifle_infantry_move_3_6");
		// Per-entity desync: id 200 offsets 200*37%2000=1400ms in -> frame 5 at t=0.
		CC_CHECK(art.UnitSprite(UnitType::RifleInfantry, true, 200, 0.0f, 6) == "rifle_infantry_move_5_6");
		// Direction 0 moves column 0 (sprites 2200..2256).
		CC_CHECK(art.UnitSprite(UnitType::RifleInfantry, true, 1, 0.0f, 0) == "rifle_infantry_move_0_0");
		CC_CHECK(art.UnitSprite(UnitType::RifleInfantry, true, 1, 0.8f, 0) == "rifle_infantry_move_3_0");
		// Out-of-range facing clamps to Right (column 6).
		CC_CHECK(art.UnitSprite(UnitType::RifleInfantry, true, 1, 0.0f, 9) == "rifle_infantry_move_0_6");
		CC_CHECK(art.UnitSprite(UnitType::RifleInfantry, false, 1, 0.0f, -1) == "rifle_infantry_idle_0_6");
		// PrototypeInfantry resolves its own namespace, not rifle_infantry's.
		CC_CHECK(art.UnitSprite(UnitType::PrototypeInfantry, false, 1, 0.0f, 0) ==
				 "prototypeinfantry_idle_0_0");
		CC_CHECK(art.UnitSprite(UnitType::PrototypeInfantry, true, 1, 0.0f, 6) ==
				 "prototypeinfantry_move_0_6");
		CC_CHECK(art.UnitSprite(UnitType::PrototypeInfantry, true, 1, 0.3f, 6) ==
				 "prototypeinfantry_move_1_6");
		// Deterministic: same args -> same frame.
		CC_CHECK(art.UnitSprite(UnitType::RifleInfantry, true, 1, 0.13f, 6) ==
				 art.UnitSprite(UnitType::RifleInfantry, true, 1, 0.13f, 6));
		// Attack anim is 4x250ms (4 fps) per direction column and wins over
		// move/idle while attacking (column 6 = right: sprites
		// 2406/2414/2422/2430).
		CC_CHECK(art.UnitSprite(UnitType::RifleInfantry, false, 1, 0.0f, 6, true) ==
				 "rifle_infantry_attack_0_6");
		CC_CHECK(art.UnitSprite(UnitType::RifleInfantry, false, 1, 0.3f, 6, true) ==
				 "rifle_infantry_attack_1_6");
		CC_CHECK(art.UnitSprite(UnitType::RifleInfantry, true, 1, 0.0f, 6, true) ==
				 "rifle_infantry_attack_0_6");
		// Types without attack anims fall back to move/idle when attacking.
		CC_CHECK(art.UnitSprite(UnitType::PrototypeInfantry, false, 1, 0.0f, 6, true) ==
				 "prototypeinfantry_idle_0_6");
		// AntiArmorInfantry resolves the antiarmor namespace (72x72 cells):
		// idle + move are 8x250ms (4 fps), attack 4x250ms per column.
		CC_CHECK(art.UnitSprite(UnitType::AntiArmorInfantry, false, 1, 0.0f, 6) ==
				 "antiarmor_idle_0_6");
		CC_CHECK(art.UnitSprite(UnitType::AntiArmorInfantry, true, 1, 0.0f, 6) ==
				 "antiarmor_move_0_6");
		CC_CHECK(art.UnitSprite(UnitType::AntiArmorInfantry, true, 1, 0.8f, 6) ==
				 "antiarmor_move_3_6");
		CC_CHECK(art.UnitSprite(UnitType::AntiArmorInfantry, false, 1, 0.0f, 6, true) ==
				 "antiarmor_attack_0_6");
		CC_CHECK(art.UnitSprite(UnitType::AntiArmorInfantry, false, 1, 0.6f, 6, true) ==
				 "antiarmor_attack_2_6");
		// Engineer resolves the engineer namespace (72x72 cells, 8-frame
		// move and 4-frame attack per column).
		CC_CHECK(art.UnitSprite(UnitType::Engineer, false, 1, 0.0f, 6) == "engineer_idle_0_6");
		CC_CHECK(art.UnitSprite(UnitType::Engineer, true, 1, 0.0f, 6) == "engineer_move_0_6");
		CC_CHECK(art.UnitSprite(UnitType::Engineer, true, 1, 0.8f, 6) == "engineer_move_3_6");
		CC_CHECK(art.UnitSprite(UnitType::Engineer, false, 1, 0.0f, 6, true) ==
				 "engineer_attack_0_6");
		// Medic resolves the medic namespace (72x72 cells, 8-frame move and
		// 4-frame attack per column).
		CC_CHECK(art.UnitSprite(UnitType::Medic, false, 1, 0.0f, 6) == "medic_idle_0_6");
		CC_CHECK(art.UnitSprite(UnitType::Medic, true, 1, 0.0f, 6) == "medic_move_0_6");
		CC_CHECK(art.UnitSprite(UnitType::Medic, true, 1, 0.8f, 6) == "medic_move_3_6");
		CC_CHECK(art.UnitSprite(UnitType::Medic, false, 1, 0.0f, 6, true) ==
				 "medic_attack_0_6");
		// Types without atlas entries resolve empty (legacy DrawUnit covers).
		CC_CHECK(art.UnitSprite(UnitType::HeavyTank, true, 1, 0.0f, 0).empty());
		CC_CHECK(art.UnitSprite(UnitType::HeavyTank, false, 1, 0.0f, 0).empty());
		// Death anims: non-looping, clamped, holding the last frame.
		CC_CHECK(art.UnitDeathAnimSeconds(UnitType::RifleInfantry) == 1.0f);
		CC_CHECK(art.UnitDeathAnimSeconds(UnitType::Medic) == 1.0f);
		CC_CHECK(art.UnitDeathAnimSeconds(UnitType::HeavyTank) == 0.0f);
		CC_CHECK(art.UnitDeathSprite(UnitType::RifleInfantry, 1, 0.0f, 6) ==
				 "rifle_infantry_death_0_6");
		CC_CHECK(art.UnitDeathSprite(UnitType::RifleInfantry, 1, 0.3f, 6) ==
				 "rifle_infantry_death_1_6");
		// 0.6s is mid-anim (4x250ms total at 4 fps).
		CC_CHECK(art.UnitDeathSprite(UnitType::RifleInfantry, 1, 0.6f, 6) ==
				 "rifle_infantry_death_2_6");
		// 1.1s is past the total: the last frame holds.
		CC_CHECK(art.UnitDeathSprite(UnitType::RifleInfantry, 1, 1.1f, 6) ==
				 "rifle_infantry_death_3_6");
		CC_CHECK(art.UnitDeathSprite(UnitType::RifleInfantry, 1, 0.0f, 9) ==
				 "rifle_infantry_death_0_6"); // facing clamps to Right
		CC_CHECK(art.UnitDeathSprite(UnitType::HeavyTank, 1, 0.0f, 6).empty());
	}

	// --- layered tank art: lighttank body/head grids, heavytank fallback ---
	{
		Art art;
		CC_CHECK(!art.HasTankArt(UnitType::LightTank));
		CC_CHECK(art.LoadAtlas().has_value());
		CC_CHECK(art.HasTankArt(UnitType::LightTank));
		CC_CHECK(!art.HasTankArt(UnitType::HeavyTank));
		CC_CHECK(art.TankBodySprite(UnitType::LightTank, 0) == "lighttank_body_idle_0_0");
		CC_CHECK(art.TankBodySprite(UnitType::LightTank, 7) == "lighttank_body_idle_0_7");
		CC_CHECK(art.TankBodySprite(UnitType::LightTank, 9) == "lighttank_body_idle_0_6");
		CC_CHECK(art.TankHeadSprite(UnitType::LightTank, 0) == "lighttank_head_0_0");
		CC_CHECK(art.TankHeadSprite(UnitType::LightTank, 3) == "lighttank_head_0_3");
		CC_CHECK(art.TankHeadSprite(UnitType::LightTank, -1) == "lighttank_head_0_6");
		CC_CHECK(art.TankBodySprite(UnitType::HeavyTank, 0).empty());
		CC_CHECK(art.TankHeadSprite(UnitType::HeavyTank, 0).empty());
		const std::expected<SpriteSheetData, ConfigError> loaded = LoadSpriteSheet();
		CC_CHECK(loaded.has_value());
		const SpriteSheetData sheet = loaded.value_or(SpriteSheetData{});
		const SpriteDefInfo *body = FindSpriteByName(sheet, "lighttank_body_idle_0_6");
		CC_CHECK(body != nullptr);
		CC_CHECK(body->maskSprite == 1556);
		Rectangle frame = {};
		CC_CHECK(!art.SpriteDrawRect("lighttank_body_idle_0_0", { 0.0f, 0.0f }, frame));
		art.DrawAtlasFrame(art.TankBodySprite(UnitType::LightTank, 0), { 0.0f, 0.0f },
						   WHITE); // no crash
	}

	// --- BaseArtScale: foot infantry renders 72px cells at 0.8x in the
	// 128px tile; everything else stays at 1x ---
	{
		CC_CHECK(Art::BaseArtScale(UnitType::PrototypeInfantry) == 1.0f);
		CC_CHECK(Art::BaseArtScale(UnitType::RifleInfantry) == 0.8f);
		CC_CHECK(Art::BaseArtScale(UnitType::AntiArmorInfantry) == 0.8f);
		CC_CHECK(Art::BaseArtScale(UnitType::Engineer) == 0.8f);
		CC_CHECK(Art::BaseArtScale(UnitType::Medic) == 0.8f);
		CC_CHECK(Art::BaseArtScale(UnitType::HeavyTank) == 1.0f);
	}

	// --- terrain tiles: stem + slot per type, none for Building ---
	{
		CC_CHECK(std::string(Art::TerrainFile(TerrainType::Grass)) == "grass");
		CC_CHECK(std::string(Art::TerrainFile(TerrainType::Water)) == "water");
		CC_CHECK(std::string(Art::TerrainFile(TerrainType::Forest)) == "forest");
		CC_CHECK(std::string(Art::TerrainFile(TerrainType::Rock)) == "rock");
		CC_CHECK(Art::TerrainFile(TerrainType::Building) == nullptr);
		CC_CHECK(Art::TerrainSlot(TerrainType::Grass) == 0);
		CC_CHECK(Art::TerrainSlot(TerrainType::Water) == 1);
		CC_CHECK(Art::TerrainSlot(TerrainType::Forest) == 2);
		CC_CHECK(Art::TerrainSlot(TerrainType::Rock) == 3);
		CC_CHECK(Art::TerrainSlot(TerrainType::Building) == -1);
		// Every art-bearing type owns a distinct slot inside terrain_.
		bool seenSlots[Art::kTerrainSlots] = {};
		for (int t = 0; t < static_cast<int>(TerrainType::Count); ++t)
		{
			const int slot = Art::TerrainSlot(static_cast<TerrainType>(t));
			if (slot < 0)
			{
				continue;
			}
			if (slot >= Art::kTerrainSlots)
			{
				CC_CHECK(false);
				continue;
			}
			CC_CHECK(!seenSlots[slot]);
			seenSlots[slot] = true;
		}
		// Headless no-op: rectangles path never touches the GPU.
		Art art;
		CC_CHECK(!art.Init(InitMode::Headless));
		art.DrawTerrain(TerrainType::Grass, { 0.0f, 0.0f });
		art.DrawTerrain(TerrainType::Building, { 0.0f, 0.0f });
		CC_CHECK(true);
	}
}
