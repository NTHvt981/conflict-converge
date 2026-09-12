// Unit tests for the M2 Goal 6 InputManager (snapshot routing).
// Update/PollLive read live raylib state; Snapshot covers the same paths.

#include "test_harness.h"

#include "InputManager.h"

void RunInputManagerTests()
{
    InputManager input;

    // --- default snapshot: centered mouse, nothing pressed ---
    CC_CHECK(input.MouseScreen().x == 0.0f);
    CC_CHECK(!input.LeftPressed());
    CC_CHECK(!input.RightPressed());

    // --- snapshot stores position + edges ---
    input.Snapshot({ 100.0f, 50.0f }, true, false);
    CC_CHECK(input.MouseScreen().x == 100.0f);
    CC_CHECK(input.MouseScreen().y == 50.0f);
    CC_CHECK(input.LeftPressed());
    CC_CHECK(!input.RightPressed());

    input.Snapshot({ 10.0f, 20.0f }, false, true);
    CC_CHECK(!input.LeftPressed());
    CC_CHECK(input.RightPressed());

    // --- world mapping runs through the camera view ---
    GameCamera camera;
    camera.view.offset = { 400.0f, 225.0f };
    camera.view.zoom = 1.0f;
    camera.view.target = { 64.0f, 64.0f };
    input.Snapshot({ 400.0f, 225.0f }, false, false);
    Vector2 world = input.MouseWorld(camera);
    CC_CHECK(CcNear(world.x, 64.0f));
    CC_CHECK(CcNear(world.y, 64.0f));

    // --- snapshot is replaced wholesale each frame, not latched ---
    input.Snapshot({ 0.0f, 0.0f }, false, false);
    CC_CHECK(!input.LeftPressed());
    CC_CHECK(!input.RightPressed());

    // --- shortcut registry rides along and fires through the manager ---
    int halts = 0;
    input.shortcuts.Bind(32, [&] { ++halts; });
    CC_CHECK(input.shortcuts.Fire(32));
    CC_CHECK(halts == 1);
}
