// Unit tests for minimap math (Init/Unload need a GPU window and
// are exercised live in main.cpp, not here).

#include "test_harness.h"

#include "Minimap.h"

void RunMinimapTests()
{
    Minimap minimap;
    minimap.screenRect = { 640.0f, 330.0f, 160.0f, 120.0f };
    minimap.refreshInterval = 0.5f;

    // --- world -> minimap mapping (20x15 map = 1280x960 world px) ---
    const Vector2 origin = minimap.WorldToMinimap({ 0.0f, 0.0f }, 20, 15);
    CC_CHECK(CcNear(origin.x, 640.0f) && CcNear(origin.y, 330.0f));
    const Vector2 mid = minimap.WorldToMinimap({ 640.0f, 480.0f }, 20, 15);
    CC_CHECK(CcNear(mid.x, 720.0f) && CcNear(mid.y, 390.0f));
    const Vector2 far = minimap.WorldToMinimap({ 1280.0f, 960.0f }, 20, 15);
    CC_CHECK(CcNear(far.x, 800.0f) && CcNear(far.y, 450.0f));

    // --- refresh cadence ---
    CC_CHECK(!minimap.PollRefresh(0.2f));
    CC_CHECK(!minimap.PollRefresh(0.2f));
    CC_CHECK(minimap.PollRefresh(0.2f)); // 0.6 >= 0.5: due, rearmed
    CC_CHECK(!minimap.PollRefresh(0.2f));
    CC_CHECK(!minimap.PollRefresh(0.0f));
    CC_CHECK(!minimap.PollRefresh(-1.0f));

    // --- viewport box: camera on origin, 800x450 screen, zoom 1 ---
    Camera2D view = { 0 };
    view.offset = { 400.0f, 225.0f };
    view.target = { 0.0f, 0.0f };
    view.zoom = 1.0f;
    const Rectangle box = minimap.ViewportRect(view, 800, 450, 20, 15);
    // Visible world is [-400,400]x[-225,225] -> minimap pixels at 0.125 scale.
    CC_CHECK(CcNear(box.x, 590.0f));
    CC_CHECK(CcNear(box.y, 301.875f));
    CC_CHECK(CcNear(box.width, 100.0f));
    CC_CHECK(CcNear(box.height, 56.25f));

    // --- letterbox: square frame, wide map -> bars top/bottom ---
    Minimap square;
    square.screenRect = { 640.0f, 330.0f, 120.0f, 120.0f };
    const Rectangle content = square.ContentRect(20, 15);
    CC_CHECK(CcNear(content.width, 120.0f));
    CC_CHECK(CcNear(content.height, 90.0f));
    CC_CHECK(CcNear(content.x, 640.0f));
    CC_CHECK(CcNear(content.y, 345.0f));
    const Vector2 sqMid = square.WorldToMinimap({ 640.0f, 480.0f }, 20, 15);
    CC_CHECK(CcNear(sqMid.x, 700.0f) && CcNear(sqMid.y, 390.0f));
    const Vector2 sqBack =
        square.MinimapToWorld(square.WorldToMinimap({ 768.0f, 576.0f }, 20, 15), 20, 15);
    CC_CHECK(CcNear(sqBack.x, 768.0f, 1.0f) && CcNear(sqBack.y, 576.0f, 1.0f));
    const Vector2 barHit = square.MinimapToWorld({ 700.0f, 332.0f }, 20, 15);
    CC_CHECK(barHit.x == 640.0f && barHit.y == 0.0f); // bar clamps into bounds

    // --- square top-right placement scales with height ---
    const Rectangle placed = Minimap::TopRightSquare(1280, 720);
    CC_CHECK(CcNear(placed.width, 240.0f) && CcNear(placed.height, 240.0f));
    CC_CHECK(CcNear(placed.x, 1030.0f) && CcNear(placed.y, 10.0f));
}
