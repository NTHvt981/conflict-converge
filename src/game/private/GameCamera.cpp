#include "GameCamera.h"

void GameCamera::UpdateWASD(float speedPixelsPerSec, float dtSeconds)
{
    Vector2 delta = { 0.0f, 0.0f };
    if (IsKeyDown(KEY_A))
    {
        delta.x -= 1.0f;
    }
    if (IsKeyDown(KEY_D))
    {
        delta.x += 1.0f;
    }
    if (IsKeyDown(KEY_W))
    {
        delta.y -= 1.0f;
    }
    if (IsKeyDown(KEY_S))
    {
        delta.y += 1.0f;
    }
    delta.x *= speedPixelsPerSec * dtSeconds;
    delta.y *= speedPixelsPerSec * dtSeconds;
    Pan(delta);
}

void GameCamera::Pan(Vector2 delta)
{
    view.target.x += delta.x;
    view.target.y += delta.y;
}

Vector2 GameCamera::ScreenToWorld(Vector2 screenPos) const
{
    return GetScreenToWorld2D(screenPos, view);
}
