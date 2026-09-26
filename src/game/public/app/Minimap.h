#pragma once

#include "raylib.h"
#include "core/Subsystem.h"

// Periodic refresh, not per-frame; unit positions draw as markers.
// The frame is square, sized by screen height; rectangle maps letterbox
// inside it (black bars), so the world never stretches.
inline constexpr float kMinimapHeightFraction = 1.0f / 3.0f;
inline constexpr float kMinimapMargin = 10.0f;

struct Minimap : public Subsystem
{
    Rectangle screenRect = { 0.0f, 0.0f, 160.0f, 120.0f };
    // Seconds between texture refreshes.
    float refreshInterval = 0.5f;
    float elapsed = 0.0f;

    RenderTexture2D target = { 0 };
    bool ready = false;

    void Init(Rectangle rect);
    void Unload();

    // Advance the timer; returns true when a refresh is due (and rearms).
    bool PollRefresh(float dt);

    Vector2 WorldToMinimap(Vector2 world, int mapW, int mapH) const;

    // Aspect-preserved map area inside the square frame; bars fill the rest.
    Rectangle ContentRect(int mapW, int mapH) const;
    // Square top-right placement sized by screen height.
    static Rectangle TopRightSquare(int screenW, int screenH);

    // Minimap pixel -> world position (click-to-move); inverse of
    // WorldToMinimap, clamped into map bounds.
    Vector2 MinimapToWorld(Vector2 minimapPx, int mapW, int mapH) const;

    // Edges inclusive.
    bool Contains(Vector2 screenPx) const;

    Rectangle ViewportRect(const Camera2D &view, int screenW, int screenH, int mapW, int mapH) const;
};
