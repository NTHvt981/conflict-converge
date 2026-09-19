#include "Minimap.h"

#include "MathUtils.h"

void Minimap::Init(Rectangle rect)
{
    Unload();
    screenRect = rect;
    target = LoadRenderTexture(static_cast<int>(rect.width), static_cast<int>(rect.height));
    elapsed = refreshInterval;
    ready = true;
}

void Minimap::Unload()
{
    if (ready)
    {
        UnloadRenderTexture(target);
        target = { 0 };
        ready = false;
    }
}

bool Minimap::PollRefresh(float dt)
{
    if (dt <= 0.0f)
    {
        return false;
    }
    elapsed += dt;
    if (elapsed >= refreshInterval)
    {
        elapsed = 0.0f;
        return true;
    }
    return false;
}

Vector2 Minimap::WorldToMinimap(Vector2 world, int mapW, int mapH) const
{
    if (mapW <= 0 || mapH <= 0 || screenRect.width <= 0.0f || screenRect.height <= 0.0f)
    {
        return { screenRect.x, screenRect.y };
    }
    const float scaleX = screenRect.width / (static_cast<float>(mapW) * cc::TILE_SIZE);
    const float scaleY = screenRect.height / (static_cast<float>(mapH) * cc::TILE_SIZE);
    return { screenRect.x + world.x * scaleX, screenRect.y + world.y * scaleY };
}

Vector2 Minimap::MinimapToWorld(Vector2 minimapPx, int mapW, int mapH) const
{
    if (mapW <= 0 || mapH <= 0 || screenRect.width <= 0.0f || screenRect.height <= 0.0f)
    {
        return { 0.0f, 0.0f };
    }
    const float worldX =
        (minimapPx.x - screenRect.x) / screenRect.width * static_cast<float>(mapW) * cc::TILE_SIZE;
    const float worldY = (minimapPx.y - screenRect.y) / screenRect.height *
                         static_cast<float>(mapH) * cc::TILE_SIZE;
    const float maxX = static_cast<float>(mapW) * cc::TILE_SIZE;
    const float maxY = static_cast<float>(mapH) * cc::TILE_SIZE;
    Vector2 clamped = { worldX < 0.0f ? 0.0f : worldX, worldY < 0.0f ? 0.0f : worldY };
    if (clamped.x > maxX)
    {
        clamped.x = maxX;
    }
    if (clamped.y > maxY)
    {
        clamped.y = maxY;
    }
    return clamped;
}

bool Minimap::Contains(Vector2 screenPx) const
{
    return screenPx.x >= screenRect.x && screenPx.x <= screenRect.x + screenRect.width &&
           screenPx.y >= screenRect.y && screenPx.y <= screenRect.y + screenRect.height;
}

Rectangle Minimap::ViewportRect(const Camera2D &view, int screenW, int screenH, int mapW, int mapH) const
{
    const Vector2 topLeft = GetScreenToWorld2D({ 0.0f, 0.0f }, view);
    const Vector2 bottomRight =
        GetScreenToWorld2D({ static_cast<float>(screenW), static_cast<float>(screenH) }, view);
    const Vector2 miniMin = WorldToMinimap(topLeft, mapW, mapH);
    const Vector2 miniMax = WorldToMinimap(bottomRight, mapW, mapH);
    return { miniMin.x, miniMin.y, miniMax.x - miniMin.x, miniMax.y - miniMin.y };
}
