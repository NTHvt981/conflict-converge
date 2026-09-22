#pragma once

#include "raylib.h"
#include "Subsystem.h"

// Manual panning camera wrapping raylib's Camera2D: WASD/edge panning plus a
// key-free Pan for tests. Named GameCamera because raylib already defines
// Camera.

class GameCamera : public Subsystem
{
public:
    static constexpr float kMinZoom = 0.5f; // absolute floor
    static constexpr float kMaxZoom = 2.0f; // absolute ceiling

    Camera2D view = {};

    // Poll WASD and pan at speed pixels/sec scaled by dt seconds.
    void UpdateWASD(float speedPixelsPerSec, float dtSeconds);

    // Edge panning: pan when the cursor is within `margin` pixels of a screen
    // edge; same speed as WASD.
    void UpdateEdgePan(float speedPixelsPerSec, float dtSeconds,
                       int screenW, int screenH, float margin = 20.0f);

    // Key-free pan by a raw screen-space delta (tests, scripted moves).
    void Pan(Vector2 delta);

    // Scroll-wheel zoom: each step scales 1.125x, clamped to [kMinZoom, kMaxZoom].
    void AdjustZoom(float wheelSteps);

    // Smallest zoom that still fills the screen with world (no void beyond
    // the map edge). Degenerate dims fall back to kMinZoom.
    float MinZoomForWorld(float mapWidthPx, float mapHeightPx, int screenW, int screenH) const;
    // Raise view.zoom to MinZoomForWorld when zoomed out past it.
    void ClampZoomToWorld(float mapWidthPx, float mapHeightPx, int screenW, int screenH);

    // Keep the view inside the world rect; small maps center instead of clamp.
    void ClampToMap(float mapWidthPx, float mapHeightPx, int screenW, int screenH);

    // Screen pixel -> world position under the current view.
    Vector2 ScreenToWorld(Vector2 screenPos) const;
};
