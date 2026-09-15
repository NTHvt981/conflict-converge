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

void GameCamera::UpdateEdgePan(float speedPixelsPerSec, float dtSeconds,
                               int screenW, int screenH, float margin)
{
    const Vector2 mouse = GetMousePosition();
    Vector2 delta = { 0.0f, 0.0f };
    if (mouse.x < margin)
    {
        delta.x = -1.0f;
    }
    else if (mouse.x > static_cast<float>(screenW) - margin)
    {
        delta.x = 1.0f;
    }
    if (mouse.y < margin)
    {
        delta.y = -1.0f;
    }
    else if (mouse.y > static_cast<float>(screenH) - margin)
    {
        delta.y = 1.0f;
    }
    if (delta.x != 0.0f || delta.y != 0.0f)
    {
        delta.x *= speedPixelsPerSec * dtSeconds;
        delta.y *= speedPixelsPerSec * dtSeconds;
        Pan(delta);
    }
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
    if (zoom < GameCamera::kMinZoom)
    {
        zoom = GameCamera::kMinZoom;
    }
    if (zoom > GameCamera::kMaxZoom)
    {
        zoom = GameCamera::kMaxZoom;
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

float GameCamera::MinZoomForWorld(float mapWidthPx, float mapHeightPx, int screenW,
                                  int screenH) const
{
    if (mapWidthPx <= 0.0f || mapHeightPx <= 0.0f || screenW <= 0 || screenH <= 0)
    {
        return kMinZoom;
    }
    const float fitW = static_cast<float>(screenW) / mapWidthPx;
    const float fitH = static_cast<float>(screenH) / mapHeightPx;
    const float fit = fitW > fitH ? fitW : fitH;
    return fit > kMaxZoom ? kMaxZoom : fit;
}

void GameCamera::ClampZoomToWorld(float mapWidthPx, float mapHeightPx, int screenW, int screenH)
{
    const float floor = MinZoomForWorld(mapWidthPx, mapHeightPx, screenW, screenH);
    if (view.zoom < floor)
    {
        view.zoom = floor;
    }
}
