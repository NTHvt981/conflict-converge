
#include "core/GameRenderer.h"

#include "economy/Building.h"
#include "units/Extensions.h"
#include "app/ui/Hud.h"
#include "core/MathUtils.h"
#include "app/ui/RmlUiHost.h"
#include "app/ui/RmlUiMenus.h"
#include "app/input/Selection.h"
#include "units/Unit.h"
#include <algorithm>
#include <string>
#include <vector>

void GameRenderer::DrawDragPreviews()
{
    if (playingInput_.IsDragging() && input_.LeftDown())
    {
        DrawRectangleLinesEx(
            NormalizeRect(playingInput_.DragStart(), input_.MouseScreen()), 1.0f, GREEN);
    }
    if (playingInput_.RepairDragActive() && input_.LeftDown() && playingInput_.AreaRepairMode())
    {
        DrawRectangleLinesEx(
            NormalizeRect(playingInput_.RepairDragStart(), input_.MouseScreen()), 1.0f, GREEN);
        std::vector<Entity> preview;
        CollectAreaRepairCandidates(
            registry_,
            DraggedWorldBox(camera_, playingInput_.RepairDragStart(), input_.MouseScreen()), 0,
            preview);
        for (const Entity id : preview)
        {
            Vector2 center = { 0.0f, 0.0f };
            if (const Unit *unit = registry_.Get<Unit>(id))
            {
                center = { unit->position.x + 32.0f, unit->position.y + 32.0f };
            }
            else if (const Building *building = registry_.Get<Building>(id))
            {
                const Rectangle fp = BuildingFootprintRect(*building);
                center = { fp.x + fp.width / 2.0f, fp.y + fp.height / 2.0f };
            }
            DrawCircleV(GetWorldToScreen2D(center, camera_.view), 4.0f, GREEN);
        }
    }
    if (playingInput_.IsRightDragging() && input_.RightDown())
    {
        DrawLineEx(playingInput_.RightDragStart(), input_.MouseScreen(), 2.0f, SKYBLUE);
        DrawCircleV(playingInput_.RightDragStart(), 3.0f, SKYBLUE);
        DrawCircleV(input_.MouseScreen(), 3.0f, SKYBLUE);
    }
}

void GameRenderer::DrawMinimapOverlays(int screenWidth, int screenHeight)
{
    if (menu_.settings.showMinimap)
    {
        DrawTextureRec(minimap_.target.texture,
                       { 0.0f, 0.0f, minimap_.screenRect.width, -minimap_.screenRect.height },
                       { minimap_.screenRect.x, minimap_.screenRect.y }, WHITE);
        DrawRectangleLinesEx(
            minimap_.ViewportRect(camera_.view, screenWidth, screenHeight, map_.Width(), map_.Height()),
            1.0f, WHITE);
        DrawRectangleLinesEx(minimap_.screenRect, 1.0f, DARKGRAY);
        for (const Ping &ping : pings_.Active())
        {
            const Vector2 blip = minimap_.WorldToMinimap(ping.worldPos, map_.Width(), map_.Height());
            const float pulse = 2.0f + ping.age * 2.0f;
            const Color color = ping.kind == PingKind::UnderAttack
                                    ? Fade(RED, 1.0f - ping.age / 5.0f)
                                    : Fade(ORANGE, 1.0f - ping.age / 5.0f);
            DrawCircleV(blip, pulse, color);
        }
    }
}

void GameRenderer::DrawHoverTooltip()
{
    if (menu_.state == MenuState::Playing && !playingInput_.IsDragging() &&
        !playingInput_.PlaceDragActive() && !playingInput_.RepairDragActive() &&
        !playingInput_.IsRightDragging() && !playingInput_.PlacingType().has_value() &&
        !playingInput_.AttackGroundMode() && !playingInput_.AreaRepairMode())
    {
        const Entity hovered = PickUnitAt(registry_, input_.MouseWorld(camera_));
        if (UpdateHoverTooltip(hoverTip_, hovered, GetFrameTime(), kHoverTooltipDelay))
        {
            if (const Unit *unit = registry_.Get<Unit>(hoverTip_.hovered))
            {
                const bool visible =
                    unit->teamID == 0 ||
                    fog_.IsVisible(0, cc::WorldToTile(cc::ToGlm(unit->position)));
                if (visible)
                {
                    const std::vector<std::string> lines = UnitTooltipLines(
                        *unit, GetOrders(registry_, hoverTip_.hovered));
                    int width = 0;
                    for (const std::string &line : lines)
                    {
                        width = std::max(width, MeasureText(line.c_str(), 14));
                    }
                    const Vector2 mouse = input_.MouseScreen();
                    const int tx = static_cast<int>(mouse.x) + 16;
                    const int ty = static_cast<int>(mouse.y) + 20;
                    DrawRectangle(tx - 4, ty - 4, width + 8,
                                  static_cast<int>(lines.size()) * 18 + 4, Fade(BLACK, 0.75f));
                    for (std::size_t i = 0; i < lines.size(); ++i)
                    {
                        Art::DrawUiText(&art_, lines[i].c_str(), tx, ty + static_cast<int>(i) * 18, 14,
                                 RAYWHITE);
                    }
                }
            }
        }
    }
    else
    {
        hoverTip_.hovered = kInvalidEntity;
        hoverTip_.time = 0.0f;
    }
}

void GameRenderer::DrawStateOverlays(int screenWidth, int screenHeight)
{
    if (menu_.state == MenuState::HotkeyRemap)
    {
        rmlUiMenus_.DrawRemapOverlay();
    }
    if (menu_.state == MenuState::ReplayViewer)
    {
        Art::DrawUiText(&art_, TextFormat("Replay %d/%d", replayCursor_ + 1, sim_.ReplayCount()), 8, 96, 16,
                 DARKGRAY);
        Art::DrawUiText(&art_, "Left/Right step - Esc exit", 8, 116, 14, Fade(DARKGRAY, 0.8f));
    }
    if (const float fade = MenuFadeAlpha(menuStateTime_); fade < 1.0f)
    {
        DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 1.0f - fade));
    }
    // Screen-space, over the world.
    rmlUi_.Render();
}
