// Unit tests for art: phase→frame mapping, rectangle fallback, atlas, squad
// slots, and terrain tiles. Headless: no GPU or audio device needed.

#include "test_harness.h"

#include "app/data/Art.h"
#include "world/TileMap.h" // TerrainType for the terrain tile mapping

void RunArtTests()
{
	CC_CHECK(FrameForPhase(AttackPhase::Ready) == UnitFrame::Idle);
	CC_CHECK(FrameForPhase(AttackPhase::WindUp) == UnitFrame::Attack);
	CC_CHECK(FrameForPhase(AttackPhase::Recover) == UnitFrame::Attack);

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

	{
		const SquadLayout inf = SquadSlots(UnitType::RifleInfantry, 42, 1.0f);
		CC_CHECK(inf.count == 5);
		CC_CHECK(inf.scale < 1.0f); // clusters shrink: 5-wide at 0.55
		CC_CHECK(inf.slots[0].x != inf.slots[3].x || inf.slots[0].y != inf.slots[3].y);

		const SquadLayout mid = SquadSlots(UnitType::RifleInfantry, 42, 0.40f);
		CC_CHECK(mid.count == 2); // 0.40 is not > 0.40
		CC_CHECK(mid.scale > inf.scale); // fewer soldiers → larger blit

		const SquadLayout low = SquadSlots(UnitType::RifleInfantry, 42, 0.05f);
		CC_CHECK(low.count == 1);
		CC_CHECK(low.scale == 1.0f);

		const SquadLayout aa = SquadSlots(UnitType::AntiArmorInfantry, 10, 0.90f);
		CC_CHECK(aa.count == 2);
		CC_CHECK(aa.scale == 0.80f);
		CC_CHECK(SquadSlots(UnitType::AntiArmorInfantry, 10, 0.40f).count == 1);

		const SquadLayout medic = SquadSlots(UnitType::Medic, 11, 0.90f);
		CC_CHECK(medic.count == 2);
		CC_CHECK(medic.scale == 0.80f);
		CC_CHECK(SquadSlots(UnitType::Medic, 11, 0.40f).count == 1);

		const SquadLayout engineer = SquadSlots(UnitType::Engineer, 20, 1.0f);
		CC_CHECK(engineer.count == 1);
		CC_CHECK(engineer.slots[0].x == 0.0f && engineer.slots[0].y == 0.0f);
		CC_CHECK(engineer.scale == 1.0f);

		const SquadLayout prototype = SquadSlots(UnitType::PrototypeInfantry, 10, 1.0f);
		CC_CHECK(prototype.count == 5);
		CC_CHECK(prototype.scale == 0.55f);
		CC_CHECK(SquadSlots(UnitType::PrototypeInfantry, 10, 0.10f).count == 1);

		const SquadLayout laser = SquadSlots(UnitType::LaserInfantry, 10, 1.0f);
		CC_CHECK(laser.count == 5);
		CC_CHECK(laser.scale == 0.55f);
		CC_CHECK(SquadSlots(UnitType::LaserInfantry, 10, 0.10f).count == 1);

		const SquadLayout tank = SquadSlots(UnitType::HeavyTank, 99, 1.0f);
		CC_CHECK(tank.count == 1);
		CC_CHECK(tank.slots[0].x == 0.0f);
		CC_CHECK(tank.slots[0].y == 0.0f);
		CC_CHECK(tank.scale == 1.0f);

		const SquadLayout a = SquadSlots(UnitType::RifleInfantry, 77, 0.90f);
		const SquadLayout b = SquadSlots(UnitType::RifleInfantry, 77, 0.90f);
		CC_CHECK(a.slots[0].x == b.slots[0].x);
		CC_CHECK(a.slots[0].y == b.slots[0].y);
		CC_CHECK(a.scale == b.scale);

		// No overlap: active slots stay >= scale * 32px apart (jitter included).
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

	{
		Art art;
		CC_CHECK(art.LoadAtlas().has_value());
		CC_CHECK(!art.UseAtlas()); // parsed, but no GPU textures headless
		// Idle anim is 8x250ms (4 fps) per direction column (column 0 = top);
		// id 1 offsets 37ms in: t=0 -> frame 0.
		CC_CHECK(art.UnitSprite(UnitType::RifleInfantry, false, 1, 0.0f, 0) == "rifle_infantry_idle_0_0");
		CC_CHECK(art.UnitSprite(UnitType::RifleInfantry, false, 1, 0.0f, 6) == "rifle_infantry_idle_0_6");
		CC_CHECK(art.UnitSprite(UnitType::RifleInfantry, false, 1, 0.0f, 3) == "rifle_infantry_idle_0_3");
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
		CC_CHECK(art.UnitSprite(UnitType::RifleInfantry, true, 1, 0.13f, 6) ==
				 art.UnitSprite(UnitType::RifleInfantry, true, 1, 0.13f, 6));
		// Attack anim is 4x250ms per direction column and wins over move/idle;
		// column 6 = right (sprites 2406/2414/2422/2430).
		CC_CHECK(art.UnitSprite(UnitType::RifleInfantry, false, 1, 0.0f, 6, true) ==
				 "rifle_infantry_attack_0_6");
		CC_CHECK(art.UnitSprite(UnitType::RifleInfantry, false, 1, 0.3f, 6, true) ==
				 "rifle_infantry_attack_1_6");
		CC_CHECK(art.UnitSprite(UnitType::RifleInfantry, true, 1, 0.0f, 6, true) ==
				 "rifle_infantry_attack_0_6");
		// Types without attack anims fall back to move/idle when attacking.
		CC_CHECK(art.UnitSprite(UnitType::PrototypeInfantry, false, 1, 0.0f, 6, true) ==
				 "prototypeinfantry_idle_0_6");
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
		CC_CHECK(art.UnitSprite(UnitType::Engineer, false, 1, 0.0f, 6) == "engineer_idle_0_6");
		CC_CHECK(art.UnitSprite(UnitType::Engineer, true, 1, 0.0f, 6) == "engineer_move_0_6");
		CC_CHECK(art.UnitSprite(UnitType::Engineer, true, 1, 0.8f, 6) == "engineer_move_3_6");
		CC_CHECK(art.UnitSprite(UnitType::Engineer, false, 1, 0.0f, 6, true) ==
				 "engineer_attack_0_6");
		CC_CHECK(art.UnitSprite(UnitType::Medic, false, 1, 0.0f, 6) == "medic_idle_0_6");
		CC_CHECK(art.UnitSprite(UnitType::Medic, true, 1, 0.0f, 6) == "medic_move_0_6");
		CC_CHECK(art.UnitSprite(UnitType::Medic, true, 1, 0.8f, 6) == "medic_move_3_6");
		CC_CHECK(art.UnitSprite(UnitType::Medic, false, 1, 0.0f, 6, true) ==
				 "medic_attack_0_6");
		// Whole-vehicle directional sheets: idle/move/attack per direction.
		CC_CHECK(art.UnitSprite(UnitType::HeavyTank, false, 1, 0.0f, 0) ==
				 "heavytank_idle_0_0");
		CC_CHECK(art.UnitSprite(UnitType::HeavyTank, true, 1, 0.0f, 0) ==
				 "heavytank_move_0_0");
		CC_CHECK(art.UnitSprite(UnitType::HeavyTank, false, 1, 0.0f, 0, true) ==
				 "heavytank_attack_0_0");
		// Death anims: non-looping, clamped, holding the last frame.
		CC_CHECK(art.UnitDeathAnimSeconds(UnitType::RifleInfantry) == 1.0f);
		CC_CHECK(art.UnitDeathAnimSeconds(UnitType::Medic) == 1.0f);
		CC_CHECK(art.UnitDeathAnimSeconds(UnitType::HeavyTank) == 1.0f);
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
		CC_CHECK(art.UnitDeathSprite(UnitType::HeavyTank, 1, 0.0f, 6) ==
				 "heavytank_death_0_6");
	}

	{
		Art art;
		CC_CHECK(art.LoadAtlas().has_value());
		CC_CHECK(art.UnitSprite(UnitType::LightTank, false, 1, 0.0f, 6) ==
				 "lighttank_idle_0_6");
		CC_CHECK(art.UnitSprite(UnitType::LightTank, true, 1, 0.8f, 6) ==
				 "lighttank_move_3_6");
		CC_CHECK(art.UnitSprite(UnitType::LightTank, false, 1, 0.0f, 6, true) ==
				 "lighttank_attack_0_6");
		CC_CHECK(art.UnitSprite(UnitType::IFV, false, 1, 0.0f, 6) == "ifv_idle_0_6");
		CC_CHECK(art.UnitSprite(UnitType::IFV, true, 1, 0.8f, 6) == "ifv_move_3_6");
		CC_CHECK(art.UnitSprite(UnitType::IFV, false, 1, 0.0f, 6, true) == "ifv_attack_0_6");
		CC_CHECK(art.UnitSprite(UnitType::Artillery, false, 1, 0.0f, 6) ==
				 "artillery_idle_0_6");
		CC_CHECK(art.UnitSprite(UnitType::Artillery, true, 1, 0.8f, 6) ==
				 "artillery_move_3_6");
		CC_CHECK(art.UnitSprite(UnitType::Artillery, false, 1, 0.0f, 6, true) ==
				 "artillery_attack_0_6");
		const std::expected<SpriteSheetData, ConfigError> loaded = LoadSpriteSheet();
		CC_CHECK(loaded.has_value());
		const SpriteSheetData sheet = loaded.value_or(SpriteSheetData{});
		const SpriteDefInfo *idle = FindSpriteByName(sheet, "lighttank_idle_0_6");
		CC_CHECK(idle != nullptr);
		CC_CHECK(idle->maskSprite == 5306);
		Rectangle frame = {};
		CC_CHECK(!art.SpriteDrawRect("lighttank_idle_0_0", { 0.0f, 0.0f }, frame));
	}

	// Foot infantry renders 72px cells at 0.8x in the 128px tile; everything
	// else stays at 1x.
	{
		CC_CHECK(Art::BaseArtScale(UnitType::PrototypeInfantry) == 1.0f);
		CC_CHECK(Art::BaseArtScale(UnitType::RifleInfantry) == 0.8f);
		CC_CHECK(Art::BaseArtScale(UnitType::AntiArmorInfantry) == 0.8f);
		CC_CHECK(Art::BaseArtScale(UnitType::Engineer) == 0.8f);
		CC_CHECK(Art::BaseArtScale(UnitType::Medic) == 0.8f);
		CC_CHECK(Art::BaseArtScale(UnitType::LaserInfantry) == 0.8f);
		CC_CHECK(Art::BaseArtScale(UnitType::HeavyTank) == 1.0f);
	}

	{
		CC_CHECK(std::string(Art::TerrainFile(TerrainType::Grass)) == "grass");
		CC_CHECK(std::string(Art::TerrainFile(TerrainType::Water)) == "water");
		CC_CHECK(std::string(Art::TerrainFile(TerrainType::Forest)) == "forest");
		CC_CHECK(std::string(Art::TerrainFile(TerrainType::Rock)) == "rock");
		CC_CHECK(Art::TerrainFile(TerrainType::Building) == nullptr);
		CC_CHECK(Art::TerrainFile(TerrainType::Dirt) == nullptr);
		CC_CHECK(Art::TerrainFile(TerrainType::Road) == nullptr);
		CC_CHECK(Art::TerrainSlot(TerrainType::Grass) == 0);
		CC_CHECK(Art::TerrainSlot(TerrainType::Water) == 1);
		CC_CHECK(Art::TerrainSlot(TerrainType::Forest) == 2);
		CC_CHECK(Art::TerrainSlot(TerrainType::Rock) == 3);
		CC_CHECK(Art::TerrainSlot(TerrainType::Building) == -1);
		CC_CHECK(Art::TerrainSlot(TerrainType::Dirt) == -1);
		CC_CHECK(Art::TerrainSlot(TerrainType::Road) == -1);
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

	{
		CC_CHECK(Art::TileSheetFor(TerrainType::Grass) == TileSheet::Grass);
		CC_CHECK(Art::TileSheetFor(TerrainType::Water) == TileSheet::Water);
		CC_CHECK(!Art::TileSheetFor(TerrainType::Forest).has_value());
		CC_CHECK(!Art::TileSheetFor(TerrainType::Rock).has_value());
		CC_CHECK(!Art::TileSheetFor(TerrainType::Building).has_value());
		CC_CHECK(Art::TileSheetFor(TerrainType::Dirt) == TileSheet::Dirt);
		CC_CHECK(Art::TileSheetFor(TerrainType::Road) == TileSheet::Road);
		CC_CHECK(std::string(Art::TileSheetFile(TileSheet::Grass)) == "grass");
		CC_CHECK(std::string(Art::TileSheetFile(TileSheet::Water)) == "water");
		CC_CHECK(std::string(Art::TileSheetFile(TileSheet::Dirt)) == "dirt");
		CC_CHECK(std::string(Art::TileSheetFile(TileSheet::Road)) == "road");

		CC_CHECK(Art::TileVariant(3, 5) == Art::TileVariant(3, 5));
		bool seen[Art::kTileFrames] = {};
		int distinct = 0;
		for (int x = 0; x < 16; ++x)
		{
			for (int y = 0; y < 16; ++y)
			{
				const int variant = Art::TileVariant(x, y);
				CC_CHECK(variant >= 0 && variant < Art::kTileFrames);
				if (variant >= 0 && variant < Art::kTileFrames && !seen[variant])
				{
					seen[variant] = true;
					++distinct;
				}
			}
		}
		CC_CHECK(distinct >= 4);

		CC_CHECK(Art::WaterFrame(0.0f) == 0);
		CC_CHECK(Art::WaterFrame(-1.0f) == 0);
		for (int i = 0; i < Art::kTileFrames; ++i)
		{
			const float seconds = (static_cast<float>(i) + 0.5f) * Art::kWaterFrameSeconds;
			CC_CHECK(Art::WaterFrame(seconds) == i);
		}
		CC_CHECK(Art::WaterFrame((static_cast<float>(Art::kTileFrames) + 2.5f) *
								 Art::kWaterFrameSeconds) == 2);
		{
			int previous = Art::WaterFrame(0.0f);
			for (int sample = 0; sample < 200; ++sample)
			{
				const int frame = Art::WaterFrame(static_cast<float>(sample) * 0.01f);
				CC_CHECK(frame >= 0 && frame < Art::kTileFrames);
				CC_CHECK(frame == 0 || frame >= previous);
				previous = frame;
			}
		}

		Art art;
		CC_CHECK(!art.Init(InitMode::Headless));
		CC_CHECK(!art.UseTileSheets());
		art.DrawTileFill(TileSheet::Grass, 0, { 0.0f, 0.0f });
		CC_CHECK(true);
	}
}
