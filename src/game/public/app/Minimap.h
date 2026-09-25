#pragma once

#include "raylib.h"
#include "Subsystem.h"

// Periodic refresh, not per-frame; unit positions draw as markers.

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

    // Minimap pixel -> world position (click-to-move); inverse of
    // WorldToMinimap, clamped into map bounds.
    Vector2 MinimapToWorld(Vector2 minimapPx, int mapW, int mapH) const;

    // Edges inclusive.
    bool Contains(Vector2 screenPx) const;

    Rectangle ViewportRect(const Camera2D &view, int screenW, int screenH, int mapW, int mapH) const;
};
