#pragma once

#include "raylib.h"
#include "core/Subsystem.h"

// Named GameCamera because raylib already defines Camera.

class GameCamera : public Subsystem
{
public:
    static constexpr float kMinZoom = 0.5f;
    static constexpr float kMaxZoom = 2.0f;

    Camera2D view = {};

    void UpdateWASD(float speedPixelsPerSec, float dtSeconds);

    void UpdateEdgePan(float speedPixelsPerSec, float dtSeconds,
                       int screenW, int screenH, float margin = 20.0f);

    // Screen-space delta.
    void Pan(Vector2 delta);

    // Scroll-wheel zoom: each step scales 1.125x, clamped to [kMinZoom, kMaxZoom].
    void AdjustZoom(float wheelSteps);

    // Smallest zoom that still fills the screen with world (no void beyond
    // the map edge). Degenerate dims fall back to kMinZoom.
    float MinZoomForWorld(float mapWidthPx, float mapHeightPx, int screenW, int screenH) const;
    void ClampZoomToWorld(float mapWidthPx, float mapHeightPx, int screenW, int screenH);

    // Keep the view inside the world rect; small maps center instead of clamp.
    void ClampToMap(float mapWidthPx, float mapHeightPx, int screenW, int screenH);

    Vector2 ScreenToWorld(Vector2 screenPos) const;
};
