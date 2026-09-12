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
}
