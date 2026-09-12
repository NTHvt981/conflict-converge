#include "Minimap.h"

#include "MathUtils.h" // cc::TILE_SIZE

void Minimap::Init(Rectangle rect)
{
    Unload();
    screenRect = rect;
    target = LoadRenderTexture(static_cast<int>(rect.width), static_cast<int>(rect.height));
    elapsed = refreshInterval; // first frame refreshes immediately
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
    const float scaleX = screenRect.width / (static_cast<float>(mapW) * cc::TILE_SIZE);
    const float scaleY = screenRect.height / (static_cast<float>(mapH) * cc::TILE_SIZE);
    return { screenRect.x + world.x * scaleX, screenRect.y + world.y * scaleY };
}

Rectangle Minimap::ViewportRect(const Camera2D &view, int screenW, int screenH, int mapW, int mapH) const
{
    // Visible world corners: screen origin and far corner through the camera.
    const Vector2 topLeft = GetScreenToWorld2D({ 0.0f, 0.0f }, view);
    const Vector2 bottomRight =
        GetScreenToWorld2D({ static_cast<float>(screenW), static_cast<float>(screenH) }, view);
    const Vector2 miniMin = WorldToMinimap(topLeft, mapW, mapH);
    const Vector2 miniMax = WorldToMinimap(bottomRight, mapW, mapH);
    return { miniMin.x, miniMin.y, miniMax.x - miniMin.x, miniMax.y - miniMin.y };
}
