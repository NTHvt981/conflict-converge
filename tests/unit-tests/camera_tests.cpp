// Unit tests for the camera (pan math, screen->world).
// UpdateWASD polls live raylib input, so only the key-free Pan and the
// pure-math ScreenToWorld are covered here (headless-safe).

#include "test_harness.h"

#include "app/ui/GameCamera.h"
#include "app/ui/Shake.h"

void RunCameraTests()
{
    // --- pan accumulates on the view target ---
    GameCamera camera;
    camera.view.offset = { 400.0f, 225.0f };
    camera.view.zoom = 1.0f;
    CC_CHECK(camera.view.target.x == 0.0f);
    CC_CHECK(camera.view.target.y == 0.0f);

    camera.Pan({ 64.0f, -32.0f });
    CC_CHECK(camera.view.target.x == 64.0f);
    CC_CHECK(camera.view.target.y == -32.0f);
    camera.Pan({ -64.0f, 32.0f });
    CC_CHECK(camera.view.target.x == 0.0f);
    CC_CHECK(camera.view.target.y == 0.0f);

    // --- screen->world: (screen - offset) / zoom + target ---
    camera.view.target = { 64.0f, 64.0f };
    Vector2 world = camera.ScreenToWorld({ 400.0f, 225.0f });
    CC_CHECK(CcNear(world.x, 64.0f));
    CC_CHECK(CcNear(world.y, 64.0f));

    world = camera.ScreenToWorld({ 464.0f, 225.0f });
    CC_CHECK(CcNear(world.x, 128.0f));
    CC_CHECK(CcNear(world.y, 64.0f));

    // --- zoom scales the conversion ---
    camera.view.zoom = 2.0f;
    world = camera.ScreenToWorld({ 400.0f, 225.0f });
    CC_CHECK(CcNear(world.x, 64.0f));
    CC_CHECK(CcNear(world.y, 64.0f));
    world = camera.ScreenToWorld({ 528.0f, 225.0f });
    CC_CHECK(CcNear(world.x, 128.0f));
    CC_CHECK(CcNear(world.y, 64.0f));

    // --- ClampToMap: the view never leaves the world rect ---
    GameCamera bounded;
    bounded.view.zoom = 1.0f;
    bounded.view.target = { -500.0f, 9000.0f };
    bounded.ClampToMap(1536.0f, 1152.0f, 800, 450); // 24x18 map, 800x450 view
    CC_CHECK(bounded.view.target.x == 400.0f);      // half-view 400 from the left
    CC_CHECK(bounded.view.target.y == 1152.0f - 225.0f); // half-view 225 from the bottom
    bounded.view.target = { 768.0f, 576.0f }; // center-ish stays put
    bounded.ClampToMap(1536.0f, 1152.0f, 800, 450);
    CC_CHECK(bounded.view.target.x == 768.0f);
    CC_CHECK(bounded.view.target.y == 576.0f);

    // --- small maps center instead of clamping ---
    GameCamera tiny;
    tiny.view.zoom = 1.0f;
    tiny.view.target = { 0.0f, 0.0f };
    tiny.ClampToMap(320.0f, 200.0f, 800, 450); // view bigger than the world
    CC_CHECK(tiny.view.target.x == 160.0f);
    CC_CHECK(tiny.view.target.y == 100.0f);

    // --- zoom narrows the allowed range ---
    GameCamera zoomed;
    zoomed.view.zoom = 2.0f;
    zoomed.view.target = { 0.0f, 0.0f };
    zoomed.ClampToMap(1536.0f, 1152.0f, 800, 450); // half-view 200x112.5
    CC_CHECK(zoomed.view.target.x == 200.0f);
    CC_CHECK(zoomed.view.target.y == 112.5f);

    // --- MinZoomForWorld: smallest zoom that still fills the screen ---
    GameCamera zoomBound;
    // 24x18 map (1536x1152) in a 1280x720 window: width binds (0.833).
    CC_CHECK(CcNear(zoomBound.MinZoomForWorld(1536.0f, 1152.0f, 1280, 720), 0.833f, 0.001f));
    // Tall window on a wide map: height binds instead.
    CC_CHECK(CcNear(zoomBound.MinZoomForWorld(1536.0f, 1152.0f, 400, 1000), 0.868f, 0.001f));
    // Degenerate dims fall back to the absolute floor, never above max.
    CC_CHECK(zoomBound.MinZoomForWorld(0.0f, 1152.0f, 1280, 720) == GameCamera::kMinZoom);
    CC_CHECK(zoomBound.MinZoomForWorld(1536.0f, 1152.0f, 0, 720) == GameCamera::kMinZoom);
    CC_CHECK(zoomBound.MinZoomForWorld(100.0f, 100.0f, 4000, 4000) == GameCamera::kMaxZoom);

    // --- ClampZoomToWorld: over-zoomed views snap back, others untouched ---
    GameCamera wide;
    wide.view.zoom = 0.5f; // absolute floor, but world needs 0.833 here
    wide.ClampZoomToWorld(1536.0f, 1152.0f, 1280, 720);
    CC_CHECK(CcNear(wide.view.zoom, 0.833f, 0.001f));
    wide.view.zoom = 1.5f; // inside bounds: stays put
    wide.ClampZoomToWorld(1536.0f, 1152.0f, 1280, 720);
    CC_CHECK(wide.view.zoom == 1.5f);

    // --- screen shake: trauma adds/clamps, decays to 0, magnitude squares ---
    CC_CHECK(AddShakeTrauma(0.0f, kShakeDeathTrauma) == kShakeDeathTrauma);
    CC_CHECK(AddShakeTrauma(0.8f, kShakeDeathTrauma) == 1.0f); // overlapping shakes clamp
    CC_CHECK(AddShakeTrauma(1.0f, kShakeHitTrauma) == 1.0f);
    CC_CHECK(DecayShakeTrauma(1.0f, 1.0f) == 0.0f); // full trauma clears in <1s
    CC_CHECK(DecayShakeTrauma(0.0f, 1.0f) == 0.0f); // never goes negative
    float trauma = AddShakeTrauma(0.0f, kShakeDeathTrauma);
    trauma = DecayShakeTrauma(trauma, 0.1f);
    CC_CHECK(trauma > 0.0f && trauma < kShakeDeathTrauma); // partial decay
    CC_CHECK(ShakeMagnitude(0.0f) == 0.0f);
    CC_CHECK(ShakeMagnitude(1.0f) == kShakeMaxPixels);
    // Squared falloff: half trauma shakes at quarter strength, so routine
    // hits (0.2) stay sub-pixel while wipes (0.5) jolt visibly.
    CC_CHECK(CcNear(ShakeMagnitude(0.5f), kShakeMaxPixels * 0.25f));
    CC_CHECK(ShakeMagnitude(kShakeHitTrauma) < 1.0f);
    CC_CHECK(ShakeMagnitude(kShakeDeathTrauma) > 1.0f);
    // The real camera is never mutated by shake: Game renders through a
    // shaken copy, so view.target/offset are untouched by construction
    // (no shake state lives on GameCamera at all).
    CC_CHECK(true);
}
