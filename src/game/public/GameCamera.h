#pragma once

#include "raylib.h" // Camera2D, Vector2

// M2 Goal 3: manual panning camera. Wraps raylib's Camera2D with WASD
// polling (live game) plus a key-free Pan for tests and scripted moves.
// Named GameCamera: raylib already defines Camera (Camera3D alias).
// Zoom/scroll and drag-pan arrive with the M2 input goals; selection (M2
// Goal 4) and the minimap (M6) consume ScreenToWorld.

class GameCamera
{
public:
    Camera2D view = {};

    // Poll WASD and pan at speed pixels/sec scaled by dt seconds.
    void UpdateWASD(float speedPixelsPerSec, float dtSeconds);

    // Key-free pan by a raw screen-space delta (tests, scripted moves).
    void Pan(Vector2 delta);

    // Screen pixel -> world position under the current view (pure math,
    // safe headless for tests).
    Vector2 ScreenToWorld(Vector2 screenPos) const;
};
