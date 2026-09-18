#pragma once

#include "raylib.h" // Camera2D, Vector2

// Manual panning camera. Wraps raylib's Camera2D with WASD polling (live
// game) plus a key-free Pan for tests and scripted moves. Named GameCamera:
// raylib already defines Camera (Camera3D alias). Zoom/scroll, drag-pan,
// selection, and the minimap consume ScreenToWorld.

class GameCamera
{
public:
    static constexpr float kMinZoom = 0.5f; // absolute floor
    static constexpr float kMaxZoom = 2.0f; // absolute ceiling

    Camera2D view = {};

    // Poll WASD and pan at speed pixels/sec scaled by dt seconds.
    void UpdateWASD(float speedPixelsPerSec, float dtSeconds);

    // Edge panning: pan the camera when the mouse cursor is within
    // `margin` pixels of any screen edge. Uses the same speed as WASD.
    // Pass the live window dimensions (GetScreenWidth/Height).
    void UpdateEdgePan(float speedPixelsPerSec, float dtSeconds,
                       int screenW, int screenH, float margin = 20.0f);

    // Key-free pan by a raw screen-space delta (tests, scripted moves).
    void Pan(Vector2 delta);

    // Scroll-wheel zoom with limits. Each wheel step scales by
    // 1.125x, clamped to [kMinZoom, kMaxZoom]; zoom anchors on the view
    // target (raylib handles the offset math in ScreenToWorld).
    void AdjustZoom(float wheelSteps);

    // World-based zoom-out limit: the smallest zoom that still fills the
    // screen with world (no void beyond the map edge). Pure math,
    // headless-safe. Degenerate dims fall back to kMinZoom; the result
    // never exceeds kMaxZoom (huge windows just pin at max zoom).
    float MinZoomForWorld(float mapWidthPx, float mapHeightPx, int screenW, int screenH) const;
    // Raise view.zoom to MinZoomForWorld when zoomed out past it. Call
    // after AdjustZoom each frame; zoomed-in views are untouched.
    void ClampZoomToWorld(float mapWidthPx, float mapHeightPx, int screenW, int screenH);

    // Camera bounds: keep the view inside the world rect so panning past
    // the map edge never shows void. Small maps center instead of clamp.
    // Pure math (headless-safe). screenW/H are live window pixels.
    void ClampToMap(float mapWidthPx, float mapHeightPx, int screenW, int screenH);

    // Screen pixel -> world position under the current view (pure math,
    // safe headless for tests).
    Vector2 ScreenToWorld(Vector2 screenPos) const;
};
