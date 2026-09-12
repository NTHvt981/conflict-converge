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
}
