#include "app/Minimap.h"

#include <algorithm>

#include "core/MathUtils.h"

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
    const Rectangle content = ContentRect(mapW, mapH);
    if (content.width <= 0.0f || content.height <= 0.0f)
    {
        return { screenRect.x, screenRect.y };
    }
    const float scaleX = content.width / (static_cast<float>(mapW) * cc::TILE_SIZE);
    const float scaleY = content.height / (static_cast<float>(mapH) * cc::TILE_SIZE);
    return { content.x + world.x * scaleX, content.y + world.y * scaleY };
}

Rectangle Minimap::ContentRect(int mapW, int mapH) const
{
    if (mapW <= 0 || mapH <= 0 || screenRect.width <= 0.0f || screenRect.height <= 0.0f)
    {
        return { screenRect.x, screenRect.y, 0.0f, 0.0f };
    }
    const float scale =
        std::min(screenRect.width / (static_cast<float>(mapW) * cc::TILE_SIZE),
                 screenRect.height / (static_cast<float>(mapH) * cc::TILE_SIZE));
    const float w = static_cast<float>(mapW) * cc::TILE_SIZE * scale;
    const float h = static_cast<float>(mapH) * cc::TILE_SIZE * scale;
    return { screenRect.x + (screenRect.width - w) * 0.5f,
             screenRect.y + (screenRect.height - h) * 0.5f, w, h };
}

Rectangle Minimap::TopRightSquare(int screenW, int screenH)
{
    const float size = static_cast<float>(screenH) * kMinimapHeightFraction;
    return { static_cast<float>(screenW) - size - kMinimapMargin, kMinimapMargin, size,
             size };
}

Vector2 Minimap::MinimapToWorld(Vector2 minimapPx, int mapW, int mapH) const
{
    const Rectangle content = ContentRect(mapW, mapH);
    if (content.width <= 0.0f || content.height <= 0.0f)
    {
        return { 0.0f, 0.0f };
    }
    const float worldX =
        (minimapPx.x - content.x) / content.width * static_cast<float>(mapW) * cc::TILE_SIZE;
    const float worldY =
        (minimapPx.y - content.y) / content.height * static_cast<float>(mapH) * cc::TILE_SIZE;
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
