// Unit tests for the M2 Goal 3 camera (pan math, screen->world).
// UpdateWASD polls live raylib input, so only the key-free Pan and the
// pure-math ScreenToWorld are covered here (headless-safe).

#include "test_harness.h"

#include "GameCamera.h"

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
}
