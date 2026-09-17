// Unit tests for M12 art (phase→frame mapping, particle pool logic,
// rectangle-fallback flag). Nothing here needs a GPU or audio device:
// Art::Init(false) loads nothing and draw calls are never made headless.

#include "test_harness.h"

#include "Art.h"

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
        art.Shutdown(); // safe without Init(true)
        CC_CHECK(art.UseRectangles());
    }

    // --- Phase 14: SquadSlots ---
    {
        std::array<Vector2, 6> offsets;
        float scale = 1.0f;

        // Infantry at full health → 5 soldiers
        const int infCount = SquadSlots(UnitType::Infantry, 42, 1.0f, offsets, scale);
        CC_CHECK(infCount == 5);
        CC_CHECK(scale < 1.0f); // clusters shrink: 5-wide at 0.55
        // All 5 offsets must be non-identical (spaced apart)
        CC_CHECK(offsets[0].x != offsets[3].x || offsets[0].y != offsets[3].y);

        // Infantry at 40% health → 2 soldiers (0.40 is not > 0.40)
        float midScale = 1.0f;
        const int midCount = SquadSlots(UnitType::Infantry, 42, 0.40f, offsets, midScale);
        CC_CHECK(midCount == 2);
        CC_CHECK(midScale > scale); // fewer soldiers → larger blit

        // Infantry at 5% health → 1 soldier at full scale
        float lowScale = 0.0f;
        const int lowCount = SquadSlots(UnitType::Infantry, 42, 0.05f, offsets, lowScale);
        CC_CHECK(lowCount == 1);
        CC_CHECK(lowScale == 1.0f);

        // AntiArmorInfantry: 2-man team above half health, 1 below.
        std::array<Vector2, 6> aOffsets;
        float aaScale = 1.0f;
        CC_CHECK(SquadSlots(UnitType::AntiArmorInfantry, 10, 0.90f, aOffsets, aaScale) == 2);
        CC_CHECK(aaScale == 0.80f);
        CC_CHECK(SquadSlots(UnitType::AntiArmorInfantry, 10, 0.40f, aOffsets, aaScale) == 1);

        // Engineer: always a single centered sprite, like a vehicle.
        std::array<Vector2, 6> eOffsets;
        float eScale = 0.0f;
        CC_CHECK(SquadSlots(UnitType::Engineer, 20, 1.0f, eOffsets, eScale) == 1);
        CC_CHECK(eOffsets[0].x == 0.0f && eOffsets[0].y == 0.0f);
        CC_CHECK(eScale == 1.0f);

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
        SquadSlots(UnitType::Infantry, 77, 0.90f, a, sa);
        SquadSlots(UnitType::Infantry, 77, 0.90f, b, sb);
        CC_CHECK(a[0].x == b[0].x);
        CC_CHECK(a[0].y == b[0].y);
        CC_CHECK(sa == sb);

        // No overlap: every pair of active slots in a full-strength
        // Infantry squad stays >= scale * 32px apart (jitter included).
        std::array<Vector2, 6> full;
        float fullScale = 1.0f;
        const int fullCount = SquadSlots(UnitType::Infantry, 1234, 1.0f, full, fullScale);
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
        CC_CHECK(art.UnitSprite(UnitType::Infantry, false, 1, 0.0f).empty());
        art.DrawAtlasFrame("infantry_idle_0_0", { 0.0f, 0.0f }, WHITE); // no crash
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
        // Idle resolves to the first idle cell.
        CC_CHECK(art.UnitSprite(UnitType::Infantry, false, 1, 0.0f) == "infantry_idle_0_0");
        // Walk anim is 8x120ms; id 1 offsets 37ms in: t=0 -> frame 0.
        CC_CHECK(art.UnitSprite(UnitType::Infantry, true, 1, 0.0f) == "infantry_walk_0_0");
        // t=130ms + 37 offset = 167 -> frame 1.
        CC_CHECK(art.UnitSprite(UnitType::Infantry, true, 1, 0.13f) == "infantry_walk_0_1");
        // Per-entity desync: id 200 offsets 200*37%960=680ms in -> frame 5 at t=0.
        CC_CHECK(art.UnitSprite(UnitType::Infantry, true, 200, 0.0f) == "infantry_walk_0_5");
        // Deterministic: same args -> same frame.
        CC_CHECK(art.UnitSprite(UnitType::Infantry, true, 1, 0.13f) ==
                 art.UnitSprite(UnitType::Infantry, true, 1, 0.13f));
        // Types without atlas entries resolve empty (legacy DrawUnit covers).
        CC_CHECK(art.UnitSprite(UnitType::HeavyTank, true, 1, 0.0f).empty());
        CC_CHECK(art.UnitSprite(UnitType::HeavyTank, false, 1, 0.0f).empty());
    }
}
