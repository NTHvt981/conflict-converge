// Unit tests for art (phase→frame mapping, particle pool logic,
// rectangle-fallback flag). Nothing here needs a GPU or audio device:
// Art::Init(false) loads nothing and draw calls are never made headless.

#include "test_harness.h"

#include "app/data/Art.h"
#include "world/TileMap.h" // TerrainType for the terrain tile mapping

void RunArtTests()
{
    // --- attack phases map to sprite frames ---
    CC_CHECK(FrameForPhase(AttackPhase::Ready) == UnitFrame::Idle);
    CC_CHECK(FrameForPhase(AttackPhase::WindUp) == UnitFrame::Attack);
    CC_CHECK(FrameForPhase(AttackPhase::Recover) == UnitFrame::Attack);

    // --- particle pool: spawn, expiry, clear ---
    {
        Particles fx;
        CC_CHECK(fx.Count() == 0);
        fx.SpawnBurst({ 100.0f, 100.0f }, RED, 10, 60.0f, 1.0f);
        CC_CHECK(fx.Count() == 10);
        fx.Update(0.5f);
        CC_CHECK(fx.Count() == 10); // half-life: all still alive
        fx.Update(0.6f);
        CC_CHECK(fx.Count() == 0); // expired and compacted
        fx.SpawnBurst({ 0.0f, 0.0f }, WHITE, 5, 10.0f, 1.0f);
        fx.Clear();
        CC_CHECK(fx.Count() == 0);
    }

    // --- pool motion keeps bursts alive while life remains ---
    {
        Particles fx;
        fx.SpawnBurst({ 0.0f, 0.0f }, WHITE, 20, 100.0f, 10.0f);
        fx.Update(1.0f);
        CC_CHECK(fx.Count() == 20);
    }

    // --- pool cap: bursts beyond kMax are dropped, never grow ---
    {
        Particles fx;
        for (int i = 0; i < 40; ++i)
        {
            fx.SpawnBurst({ 0.0f, 0.0f }, WHITE, 50, 10.0f, 10.0f);
        }
        CC_CHECK(fx.Count() == Particles::kMax);
        fx.Update(11.0f);
        CC_CHECK(fx.Count() == 0);
    }

    // --- damage numbers: spawn, rise, expiry, clear ---
    {
        DamageNumbers dn;
        CC_CHECK(dn.Count() == 0);
        dn.Spawn({ 100.0f, 100.0f }, 25.0f);
        CC_CHECK(dn.Count() == 1);
        dn.Update(DamageNumbers::kLife / 2.0f);
        CC_CHECK(dn.Count() == 1); // half-life: still alive (and risen)
        dn.Update(DamageNumbers::kLife);
        CC_CHECK(dn.Count() == 0); // expired and compacted
        dn.Spawn({ 0.0f, 0.0f }, 1.0f);
        dn.Clear();
        CC_CHECK(dn.Count() == 0);
    }

    // --- damage pool cap: spawns beyond kMax are dropped, never grow ---
    {
        DamageNumbers dn;
        for (int i = 0; i < static_cast<int>(DamageNumbers::kMax) + 10; ++i)
        {
            dn.Spawn({ 0.0f, 0.0f }, 1.0f);
        }
        CC_CHECK(dn.Count() == DamageNumbers::kMax);
        dn.Update(DamageNumbers::kLife + 1.0f);
        CC_CHECK(dn.Count() == 0);
    }

    // --- zero-count and zero-life bursts are safe ---
    {
        Particles fx;
        fx.SpawnBurst({ 0.0f, 0.0f }, WHITE, 0, 10.0f, 1.0f);
        CC_CHECK(fx.Count() == 0);
        fx.SpawnBurst({ 0.0f, 0.0f }, WHITE, 3, 10.0f, 0.0f);
        CC_CHECK(fx.Count() == 3); // maxLife guards the divide
        fx.Update(0.01f);
        CC_CHECK(fx.Count() == 0);
    }

    // --- headless art stays on rectangles ---
    {
        Art art;
        CC_CHECK(!art.Init(false));
        CC_CHECK(!art.IsReady());
        CC_CHECK(art.UseRectangles());
        CC_CHECK(!art.HasFont()); // UI typeface loads on the device path only
        art.Shutdown(); // safe without Init(true)
        CC_CHECK(art.UseRectangles());
        CC_CHECK(!art.HasFont());
    }

    // --- : SquadSlots ---
    {
        std::array<Vector2, 6> offsets;
        float scale = 1.0f;

        // RifleInfantry at full health → 5 soldiers
        const int infCount = SquadSlots(UnitType::RifleInfantry, 42, 1.0f, offsets, scale);
        CC_CHECK(infCount == 5);
        CC_CHECK(scale < 1.0f); // clusters shrink: 5-wide at 0.55
        // All 5 offsets must be non-identical (spaced apart)
        CC_CHECK(offsets[0].x != offsets[3].x || offsets[0].y != offsets[3].y);

        // RifleInfantry at 40% health → 2 soldiers (0.40 is not > 0.40)
        float midScale = 1.0f;
        const int midCount = SquadSlots(UnitType::RifleInfantry, 42, 0.40f, offsets, midScale);
        CC_CHECK(midCount == 2);
        CC_CHECK(midScale > scale); // fewer soldiers → larger blit

        // RifleInfantry at 5% health → 1 soldier at full scale
        float lowScale = 0.0f;
        const int lowCount = SquadSlots(UnitType::RifleInfantry, 42, 0.05f, offsets, lowScale);
        CC_CHECK(lowCount == 1);
        CC_CHECK(lowScale == 1.0f);

        // AntiArmorInfantry: 2-man team above half health, 1 below.
        std::array<Vector2, 6> aOffsets;
        float aaScale = 1.0f;
        CC_CHECK(SquadSlots(UnitType::AntiArmorInfantry, 10, 0.90f, aOffsets, aaScale) == 2);
        CC_CHECK(aaScale == 0.80f);
        CC_CHECK(SquadSlots(UnitType::AntiArmorInfantry, 10, 0.40f, aOffsets, aaScale) == 1);

        // Medic: 2-man team above half health, 1 below (like AntiArmor).
        std::array<Vector2, 6> mOffsets;
        float mScale = 1.0f;
        CC_CHECK(SquadSlots(UnitType::Medic, 11, 0.90f, mOffsets, mScale) == 2);
        CC_CHECK(mScale == 0.80f);
        CC_CHECK(SquadSlots(UnitType::Medic, 11, 0.40f, mOffsets, mScale) == 1);

        // Engineer: always a single centered sprite, like a vehicle.
        std::array<Vector2, 6> eOffsets;
        float eScale = 0.0f;
        CC_CHECK(SquadSlots(UnitType::Engineer, 20, 1.0f, eOffsets, eScale) == 1);
        CC_CHECK(eOffsets[0].x == 0.0f && eOffsets[0].y == 0.0f);
        CC_CHECK(eScale == 1.0f);

        // PrototypeInfantry clusters exactly like RifleInfantry (5 at full health).
        std::array<Vector2, 6> pOffsets;
        float pScale = 0.0f;
        CC_CHECK(SquadSlots(UnitType::PrototypeInfantry, 10, 1.0f, pOffsets, pScale) == 5);
        CC_CHECK(pScale == 0.55f);
        CC_CHECK(SquadSlots(UnitType::PrototypeInfantry, 10, 0.10f, pOffsets, pScale) == 1);

        // Non-infantry always returns 1 with offset {0,0}
        std::array<Vector2, 6> vOffsets;
        float vScale = 0.0f;
        const int tankCount = SquadSlots(UnitType::HeavyTank, 99, 1.0f, vOffsets, vScale);
        CC_CHECK(tankCount == 1);
        CC_CHECK(vOffsets[0].x == 0.0f);
        CC_CHECK(vOffsets[0].y == 0.0f);
        CC_CHECK(vScale == 1.0f);

        // Deterministic: same id + health → same offsets
        std::array<Vector2, 6> a, b;
        float sa = 0.0f, sb = 0.0f;
        SquadSlots(UnitType::RifleInfantry, 77, 0.90f, a, sa);
        SquadSlots(UnitType::RifleInfantry, 77, 0.90f, b, sb);
        CC_CHECK(a[0].x == b[0].x);
        CC_CHECK(a[0].y == b[0].y);
        CC_CHECK(sa == sb);

        // No overlap: every pair of active slots in a full-strength
        // RifleInfantry squad stays >= scale * 32px apart (jitter included).
        std::array<Vector2, 6> full;
        float fullScale = 1.0f;
        const int fullCount = SquadSlots(UnitType::RifleInfantry, 1234, 1.0f, full, fullScale);
        CC_CHECK(fullCount == 5);
        for (int i = 0; i < fullCount; ++i)
        {
            for (int j = i + 1; j < fullCount; ++j)
            {
                const float dx = full[i].x - full[j].x;
                const float dy = full[i].y - full[j].y;
                CC_CHECK(dx * dx + dy * dy >= fullScale * 32.0f * fullScale * 32.0f);
            }
        }
    }

    // --- atlas: down headless, draw is a safe no-op ---
    {
        Art art;
        CC_CHECK(!art.UseAtlas());
        CC_CHECK(art.UnitSprite(UnitType::RifleInfantry, false, 1, 0.0f, 6).empty());
        art.DrawAtlasFrame("rifle_infantry_idle_0_0", { 0.0f, 0.0f }, WHITE); // no crash
        // Default mode: raw BLUE/RED (colorBlindMode_ defaults to false).
        const Color blue = art.TeamTint(0);
        CC_CHECK(blue.r == BLUE.r && blue.g == BLUE.g && blue.b == BLUE.b);
        const Color red = art.TeamTint(1);
        CC_CHECK(red.r == RED.r && red.g == RED.g && red.b == RED.b);
        // Color-blind mode: Okabe-Ito orange / sky blue.
        art.SetColorBlindMode(true);
        const Color cbOrange = art.TeamTint(0);
        CC_CHECK(cbOrange.r == 230 && cbOrange.g == 159 && cbOrange.b == 0);
        const Color cbSky = art.TeamTint(1);
        CC_CHECK(cbSky.r == 86 && cbSky.g == 180 && cbSky.b == 233);
        art.SetColorBlindMode(false);
        const Color backToBlue = art.TeamTint(0);
        CC_CHECK(backToBlue.r == BLUE.r && backToBlue.g == BLUE.g && backToBlue.b == BLUE.b);
    }

    // --- atlas: idle sprite + walk cycle from the data/configs atlas ---
    {
        Art art;
        CC_CHECK(art.LoadAtlas());
        CC_CHECK(!art.UseAtlas()); // parsed, but no GPU textures headless
        // Idle resolves to the facing column's pose (column 0 = top).
        CC_CHECK(art.UnitSprite(UnitType::RifleInfantry, false, 1, 0.0f, 0) == "rifle_infantry_idle_0_0");
        CC_CHECK(art.UnitSprite(UnitType::RifleInfantry, false, 1, 0.0f, 6) == "rifle_infantry_idle_0_6");
        CC_CHECK(art.UnitSprite(UnitType::RifleInfantry, false, 1, 0.0f, 3) == "rifle_infantry_idle_0_3");
        // Walk anim is 4x120ms per direction column (column 6 = right);
        // id 1 offsets 37ms in: t=0 -> frame 0.
        CC_CHECK(art.UnitSprite(UnitType::RifleInfantry, true, 1, 0.0f, 6) == "rifle_infantry_walk_0_6");
        // t=130ms + 37 offset = 167 -> frame 1.
        CC_CHECK(art.UnitSprite(UnitType::RifleInfantry, true, 1, 0.13f, 6) == "rifle_infantry_walk_1_6");
        // Per-entity desync: id 200 offsets 200*37%480=200ms in -> frame 1 at t=0.
        CC_CHECK(art.UnitSprite(UnitType::RifleInfantry, true, 200, 0.0f, 6) == "rifle_infantry_walk_1_6");
        // Direction 0 walks column 0 (sprites 100/108/116/124).
        CC_CHECK(art.UnitSprite(UnitType::RifleInfantry, true, 1, 0.0f, 0) == "rifle_infantry_walk_0_0");
        CC_CHECK(art.UnitSprite(UnitType::RifleInfantry, true, 1, 0.13f, 0) == "rifle_infantry_walk_1_0");
        // Out-of-range facing clamps to Right (column 6).
        CC_CHECK(art.UnitSprite(UnitType::RifleInfantry, true, 1, 0.0f, 9) == "rifle_infantry_walk_0_6");
        CC_CHECK(art.UnitSprite(UnitType::RifleInfantry, false, 1, 0.0f, -1) == "rifle_infantry_idle_0_6");
        // PrototypeInfantry resolves its own namespace, not rifle_infantry's.
        CC_CHECK(art.UnitSprite(UnitType::PrototypeInfantry, false, 1, 0.0f, 0) ==
                 "prototypeinfantry_idle_0_0");
        CC_CHECK(art.UnitSprite(UnitType::PrototypeInfantry, true, 1, 0.0f, 6) ==
                 "prototypeinfantry_walk_0_6");
        CC_CHECK(art.UnitSprite(UnitType::PrototypeInfantry, true, 1, 0.13f, 6) ==
                 "prototypeinfantry_walk_1_6");
        // Deterministic: same args -> same frame.
        CC_CHECK(art.UnitSprite(UnitType::RifleInfantry, true, 1, 0.13f, 6) ==
                 art.UnitSprite(UnitType::RifleInfantry, true, 1, 0.13f, 6));
        // Attack anim is 2x120ms per direction column and wins over
        // walk/idle while attacking (column 6 = right: sprites 406/414).
        CC_CHECK(art.UnitSprite(UnitType::RifleInfantry, false, 1, 0.0f, 6, true) ==
                 "rifle_infantry_attack_0_6");
        CC_CHECK(art.UnitSprite(UnitType::RifleInfantry, false, 1, 0.13f, 6, true) ==
                 "rifle_infantry_attack_1_6");
        CC_CHECK(art.UnitSprite(UnitType::RifleInfantry, true, 1, 0.0f, 6, true) ==
                 "rifle_infantry_attack_0_6");
        // Types without attack anims fall back to walk/idle when attacking.
        CC_CHECK(art.UnitSprite(UnitType::PrototypeInfantry, false, 1, 0.0f, 6, true) ==
                 "prototypeinfantry_idle_0_6");
        // AntiArmorInfantry resolves the antiarmor namespace (32x48 cells):
        // idle is sprite-direct, walk is 4x120ms, attack 3x120ms per column.
        CC_CHECK(art.UnitSprite(UnitType::AntiArmorInfantry, false, 1, 0.0f, 6) ==
                 "antiarmor_idle_0_6");
        CC_CHECK(art.UnitSprite(UnitType::AntiArmorInfantry, true, 1, 0.0f, 6) ==
                 "antiarmor_walk_0_6");
        CC_CHECK(art.UnitSprite(UnitType::AntiArmorInfantry, true, 1, 0.13f, 6) ==
                 "antiarmor_walk_1_6");
        CC_CHECK(art.UnitSprite(UnitType::AntiArmorInfantry, false, 1, 0.0f, 6, true) ==
                 "antiarmor_attack_0_6");
        CC_CHECK(art.UnitSprite(UnitType::AntiArmorInfantry, false, 1, 0.25f, 6, true) ==
                 "antiarmor_attack_2_6");
        // Engineer resolves the engineer namespace (32x32 cells, 4-frame
        // walk; no attack sheets, so attacking falls back to walk/idle).
        CC_CHECK(art.UnitSprite(UnitType::Engineer, false, 1, 0.0f, 6) == "engineer_idle_0_6");
        CC_CHECK(art.UnitSprite(UnitType::Engineer, true, 1, 0.0f, 6) == "engineer_walk_0_6");
        CC_CHECK(art.UnitSprite(UnitType::Engineer, true, 1, 0.13f, 6) == "engineer_walk_1_6");
        CC_CHECK(art.UnitSprite(UnitType::Engineer, false, 1, 0.0f, 6, true) ==
                 "engineer_idle_0_6");
        // Medic resolves the medic namespace (32x32 cells, 4-frame walk;
        // no attack sheets, so attacking falls back to walk/idle).
        CC_CHECK(art.UnitSprite(UnitType::Medic, false, 1, 0.0f, 6) == "medic_idle_0_6");
        CC_CHECK(art.UnitSprite(UnitType::Medic, true, 1, 0.0f, 6) == "medic_walk_0_6");
        CC_CHECK(art.UnitSprite(UnitType::Medic, true, 1, 0.13f, 6) == "medic_walk_1_6");
        CC_CHECK(art.UnitSprite(UnitType::Medic, false, 1, 0.0f, 6, true) ==
                 "medic_idle_0_6");
        // Types without atlas entries resolve empty (legacy DrawUnit covers).
        CC_CHECK(art.UnitSprite(UnitType::HeavyTank, true, 1, 0.0f, 0).empty());
        CC_CHECK(art.UnitSprite(UnitType::HeavyTank, false, 1, 0.0f, 0).empty());
    }

    // --- BaseArtScale: prototype infantry's 2x is baked into its 32px art ---
    {
        CC_CHECK(Art::BaseArtScale(UnitType::PrototypeInfantry) == 1.0f);
        CC_CHECK(Art::BaseArtScale(UnitType::RifleInfantry) == 1.0f);
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
        // Headless no-op: rectangles path never touches the GPU.
        Art art;
        CC_CHECK(!art.Init(false));
        art.DrawTerrain(TerrainType::Grass, { 0.0f, 0.0f });
        art.DrawTerrain(TerrainType::Building, { 0.0f, 0.0f });
        CC_CHECK(true);
    }
}
