#pragma once

#include "raylib.h" // Rectangle, Vector2, Camera2D, RenderTexture2D

// M6 Goal 1: minimap. The map is re-rendered into a small texture at a fixed
// cadence (periodic refresh, not per-frame); unit positions are drawn as
// markers on every refresh. Terrain features are simplified to flat blocks —
// the minimap shows positions, not detail. All math is render-free and tested;
// main.cpp owns the actual refresh drawing.

struct Minimap
{
    // Screen-space box the minimap occupies.
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

    // World position -> pixel inside screenRect for a map of mapW x mapH tiles.
    Vector2 WorldToMinimap(Vector2 world, int mapW, int mapH) const;

    // Camera view frustum expressed in minimap pixels (for the viewport box).
    Rectangle ViewportRect(const Camera2D &view, int screenW, int screenH, int mapW, int mapH) const;
};
