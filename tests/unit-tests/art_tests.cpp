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

        // Infantry at full health → 5 soldiers
        const int infCount = SquadSlots(UnitType::Infantry, 42, 1.0f, offsets);
        CC_CHECK(infCount == 5);
        // All 5 offsets must be non-identical (spaced apart)
        CC_CHECK(offsets[0].x != offsets[3].x || offsets[0].y != offsets[3].y);

        // Infantry at 40% health → 2 soldiers (0.40 is not > 0.40)
        const int midCount = SquadSlots(UnitType::Infantry, 42, 0.40f, offsets);
        CC_CHECK(midCount == 2);

        // Infantry at 5% health → 1 soldier
        const int lowCount = SquadSlots(UnitType::Infantry, 42, 0.05f, offsets);
        CC_CHECK(lowCount == 1);

        // AntiArmorInfantry: 2-man team above half health, 1 below.
        std::array<Vector2, 6> aOffsets;
        CC_CHECK(SquadSlots(UnitType::AntiArmorInfantry, 10, 0.90f, aOffsets) == 2);
        CC_CHECK(SquadSlots(UnitType::AntiArmorInfantry, 10, 0.40f, aOffsets) == 1);

        // Engineer: always a single centered sprite, like a vehicle.
        std::array<Vector2, 6> eOffsets;
        CC_CHECK(SquadSlots(UnitType::Engineer, 20, 1.0f, eOffsets) == 1);
        CC_CHECK(eOffsets[0].x == 0.0f && eOffsets[0].y == 0.0f);

        // Non-infantry always returns 1 with offset {0,0}
        std::array<Vector2, 6> vOffsets;
        const int tankCount = SquadSlots(UnitType::HeavyTank, 99, 1.0f, vOffsets);
        CC_CHECK(tankCount == 1);
        CC_CHECK(vOffsets[0].x == 0.0f);
        CC_CHECK(vOffsets[0].y == 0.0f);

        // Deterministic: same id + health → same offsets
        std::array<Vector2, 6> a, b;
        SquadSlots(UnitType::Infantry, 77, 0.90f, a);
        SquadSlots(UnitType::Infantry, 77, 0.90f, b);
        CC_CHECK(a[0].x == b[0].x);
        CC_CHECK(a[0].y == b[0].y);
    }

    // --- atlas: down headless, draw is a safe no-op ---
    {
        Art art;
        CC_CHECK(!art.UseAtlas());
        CC_CHECK(art.UnitSprite(UnitType::Infantry, false, 1, 0.0f).empty());
        art.DrawAtlasFrame("infantry_idle_0_0", { 0.0f, 0.0f }, WHITE); // no crash
        const Color blue = Art::TeamTint(0);
        CC_CHECK(blue.r == BLUE.r && blue.g == BLUE.g && blue.b == BLUE.b);
        const Color red = Art::TeamTint(1);
        CC_CHECK(red.r == RED.r && red.g == RED.g && red.b == RED.b);
    }

    // --- atlas: idle sprite + walk cycle from data/sprites.json ---
    {
        Art art;
        CC_CHECK(art.LoadAtlas());
        CC_CHECK(!art.UseAtlas()); // parsed, but no GPU textures headless
        // Idle resolves to the first idle cell.
        CC_CHECK(art.UnitSprite(UnitType::Infantry, false, 1, 0.0f) == "infantry_idle_0_0");
        // Walk anim is 4x120ms; id 1 offsets 37ms in: t=0 -> frame 0.
        CC_CHECK(art.UnitSprite(UnitType::Infantry, true, 1, 0.0f) == "infantry_walk_0_0");
        // t=130ms + 37 offset = 167 -> frame 1.
        CC_CHECK(art.UnitSprite(UnitType::Infantry, true, 1, 0.13f) == "infantry_walk_0_1");
        // Per-entity desync: id 200 offsets 200ms in -> frame 1 at t=0.
        CC_CHECK(art.UnitSprite(UnitType::Infantry, true, 200, 0.0f) == "infantry_walk_0_1");
        // Deterministic: same args -> same frame.
        CC_CHECK(art.UnitSprite(UnitType::Infantry, true, 1, 0.13f) ==
                 art.UnitSprite(UnitType::Infantry, true, 1, 0.13f));
        // Types without atlas entries resolve empty (legacy DrawUnit covers).
        CC_CHECK(art.UnitSprite(UnitType::HeavyTank, true, 1, 0.0f).empty());
        CC_CHECK(art.UnitSprite(UnitType::HeavyTank, false, 1, 0.0f).empty());
    }
}
