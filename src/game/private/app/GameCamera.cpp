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

void GameCamera::AdjustZoom(float wheelSteps)
{
    if (wheelSteps == 0.0f)
    {
        return;
    }
    float zoom = view.zoom <= 0.0f ? 1.0f : view.zoom;
    while (wheelSteps >= 1.0f)
    {
        zoom *= 1.125f;
        wheelSteps -= 1.0f;
    }
    while (wheelSteps <= -1.0f)
    {
        zoom /= 1.125f;
        wheelSteps += 1.0f;
    }
    if (zoom < 0.5f)
    {
        zoom = 0.5f;
    }
    if (zoom > 2.0f)
    {
        zoom = 2.0f;
    }
    view.zoom = zoom;
}

void GameCamera::ClampToMap(float mapWidthPx, float mapHeightPx, int screenW, int screenH)
{
    const float zoom = view.zoom <= 0.0f ? 1.0f : view.zoom;
    const float halfW = (static_cast<float>(screenW) / 2.0f) / zoom;
    const float halfH = (static_cast<float>(screenH) / 2.0f) / zoom;
    if (mapWidthPx <= halfW * 2.0f)
    {
        view.target.x = mapWidthPx / 2.0f;
    }
    else if (view.target.x < halfW)
    {
        view.target.x = halfW;
    }
    else if (view.target.x > mapWidthPx - halfW)
    {
        view.target.x = mapWidthPx - halfW;
    }
    if (mapHeightPx <= halfH * 2.0f)
    {
        view.target.y = mapHeightPx / 2.0f;
    }
    else if (view.target.y < halfH)
    {
        view.target.y = halfH;
    }
    else if (view.target.y > mapHeightPx - halfH)
    {
        view.target.y = mapHeightPx - halfH;
    }
}
