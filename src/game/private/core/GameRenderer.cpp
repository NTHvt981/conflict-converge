
#include "GameRenderer.h"

#include "raygui.h"
#include "Building.h"
#include "Cheats.h"
#include "Cursor.h"
#include "Hud.h"
#include "Selection.h"
#include "Unit.h"
#include <algorithm>
#include <vector>

DEFINE_CHEAT_WIDGET(SHOW_UNIT_ID_TOGGLE, TOGGLE, false)

namespace
{
Rectangle SquadSelectionBox(const Art &art, const Unit &unit, Entity id, bool atlasPath,
                            Rectangle body)
{
    if (!atlasPath)
    {
        return body;
    }
    std::array<Vector2, 6> slots;
    float slotScale = 1.0f;
    const int count = SquadSlots(unit.type, id, UnitHealthFraction(unit), slots, slotScale);
    const bool atlasCells =
        art.UseAtlas() && !art.UnitSprite(unit.type, false, id, 0.0f, 0).empty();
    const float s = Art::BaseArtScale(unit.type);
    float x0, y0, x1, y1;
    if (atlasCells)
    {
        x0 = unit.position.x + slots[0].x + 32.0f - 8.0f * s;
        y0 = unit.position.y + slots[0].y + 32.0f - 16.0f * s;
        x1 = unit.position.x + slots[0].x + 32.0f + 8.0f * s;
        y1 = unit.position.y + slots[0].y + 32.0f + 16.0f * s;
    }
    else
    {
        x0 = unit.position.x + slots[0].x + 16.0f;
        y0 = unit.position.y + slots[0].y + 16.0f;
        x1 = x0 + 32.0f;
        y1 = y0 + 32.0f;
    }
    for (int i = 1; i < count; ++i)
    {
        float cx0, cy0, cx1, cy1;
        if (atlasCells)
        {
            cx0 = unit.position.x + slots[i].x + 32.0f - 8.0f * s;
            cy0 = unit.position.y + slots[i].y + 32.0f - 16.0f * s;
            cx1 = unit.position.x + slots[i].x + 32.0f + 8.0f * s;
            cy1 = unit.position.y + slots[i].y + 32.0f + 16.0f * s;
        }
        else
        {
            cx0 = unit.position.x + slots[i].x + 16.0f;
            cy0 = unit.position.y + slots[i].y + 16.0f;
            cx1 = cx0 + 32.0f;
            cy1 = cy0 + 32.0f;
        }
        x0 = std::min(x0, cx0);
        y0 = std::min(y0, cy0);
        x1 = std::max(x1, cx1);
        y1 = std::max(y1, cy1);
    }
    return { x0 - 2.0f, y0 - 2.0f, (x1 - x0) + 4.0f, (y1 - y0) + 4.0f };
}
const char *DifficultyName(AIDifficulty difficulty)
{
    switch (difficulty)
    {
    case AIDifficulty::Easy:
        return "Easy";
    case AIDifficulty::Hard:
        return "Hard";
    case AIDifficulty::Medium:
    default:
        return "Medium";
    }
}
}

GameRenderer::GameRenderer(Art &art, GameCamera &camera, TileMap &map, Registry &registry,
                           FogOfWar &fog, ResourceNodes &nodes, Minimap &minimap, Pings &pings,
                           DamageNumbers &damageNumbers, PlayingInput &playingInput, MenuFlow &menu,
                           Simulation &sim, ResourceSystem &resources, ProductionQueue &queue,
                           HotkeyMap &hotkeys, InputManager &input, AICommander &ai,
                           MenuScreens &menuScreens, EventDispatcher &events, const bool &showHints,
                           const int &replayCursor, const AIDifficulty &worldDifficulty,
                           const float &menuStateTime, const float &shakeTrauma, bool &playerAutoRepair,
                           float &autoRepairCap, std::function<void()> quitToMenu)
    : art_(art)
    , camera_(camera)
    , map_(map)
    , registry_(registry)
    , fog_(fog)
    , nodes_(nodes)
    , minimap_(minimap)
    , pings_(pings)
    , damageNumbers_(damageNumbers)
    , playingInput_(playingInput)
    , menu_(menu)
    , sim_(sim)
    , resources_(resources)
    , queue_(queue)
    , hotkeys_(hotkeys)
    , input_(input)
    , ai_(ai)
    , menuScreens_(menuScreens)
    , events_(events)
    , showHints_(showHints)
    , replayCursor_(replayCursor)
    , worldDifficulty_(worldDifficulty)
    , menuStateTime_(menuStateTime)
    , shakeTrauma_(shakeTrauma)
    , playerAutoRepair_(playerAutoRepair)
    , autoRepairCap_(autoRepairCap)
    , quitToMenu_(std::move(quitToMenu))
{
}

void GameRenderer::Announce(EventType type)
{
    Event bare;
    bare.type = type;
    events_.Dispatch(bare);
}

void GameRenderer::DrawWorld()
{
    {
        CursorIntent intent = CursorIntent::Default;
        if (playingInput_.PlacingType().has_value())
        {
            const cc::IVec2 tile = cc::WorldToTile(cc::ToGlm(input_.MouseWorld(camera_)));
            intent = CanPlaceBuilding(map_, &nodes_, *playingInput_.PlacingType(), tile.x, tile.y)
                         ? CursorIntent::Default
                         : CursorIntent::InvalidPlacement;
        }
        else if (playingInput_.AttackGroundMode())
        {
            intent = CursorIntent::Attack;
        }
        else
        {
            const Entity selectedId = SelectedUnit(registry_);
            const Unit *selected =
                selectedId != kInvalidEntity ? registry_.Get<Unit>(selectedId) : nullptr;
            intent = PredictCursorIntent(registry_, selected, input_.MouseWorld(camera_));
        }
        switch (intent)
        {
        case CursorIntent::Attack:
            SetMouseCursor(MOUSE_CURSOR_CROSSHAIR);
            break;
        case CursorIntent::Repair:
            SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
            break;
        case CursorIntent::InvalidPlacement:
            SetMouseCursor(MOUSE_CURSOR_NOT_ALLOWED);
            break;
        case CursorIntent::Move:
            SetMouseCursor(MOUSE_CURSOR_ARROW);
            break;
        case CursorIntent::Default:
        default:
            SetMouseCursor(MOUSE_CURSOR_DEFAULT);
            break;
        }
    }

    Camera2D shaken = camera_.view;
    if (shakeTrauma_ > 0.0f)
    {
        const float amount = ShakeMagnitude(shakeTrauma_);
        shaken.offset.x += static_cast<float>(GetRandomValue(-1000, 1000)) / 1000.0f * amount;
        shaken.offset.y += static_cast<float>(GetRandomValue(-1000, 1000)) / 1000.0f * amount;
    }
    BeginMode2D(shaken);

    for (int y = 0; y < map_.Height(); ++y)
    {
        for (int x = 0; x < map_.Width(); ++x)
        {
            const Vector2 corner = cc::ToRaylib(cc::TileToWorld(x, y));
            const TerrainType terrain = map_.Get({ x, y });
            if (art_.UseRectangles())
            {
                if (terrain == TerrainType::Water)
                {
                    DrawRectangleV(corner, { cc::TILE_SIZE, cc::TILE_SIZE }, SKYBLUE);
                }
                else if (terrain == TerrainType::Forest)
                {
                    DrawRectangleV(corner, { cc::TILE_SIZE, cc::TILE_SIZE }, DARKGREEN);
                }
                else if (terrain == TerrainType::Rock)
                {
                    DrawRectangleV(corner, { cc::TILE_SIZE, cc::TILE_SIZE }, GRAY);
                }
                DrawRectangleLinesEx({ corner.x, corner.y, cc::TILE_SIZE, cc::TILE_SIZE }, 1.0f,
                                     LIGHTGRAY);
            }
            else
            {
                art_.DrawTerrain(terrain, corner);
            }
        }
    }

    registry_.Each<Building>([&](Entity, const Building &building) {
        if (building.teamID != 0 &&
            !fog_.IsVisible(0, cc::IVec2{ building.tileX, building.tileY }))
        {
            return;
        }
        const cc::IVec2 size = Footprint(building.type);
        const Vector2 corner = cc::ToRaylib(cc::TileToWorld(building.tileX, building.tileY));
        if (art_.UseRectangles())
        {
            const float w = static_cast<float>(size.x) * cc::TILE_SIZE;
            const float h = static_cast<float>(size.y) * cc::TILE_SIZE;
            const Color tint =
                building.type == BuildingType::Base ? DARKGRAY : building.type == BuildingType::Factory ? BROWN : GRAY;
            DrawRectangleV(corner, { w, h }, tint);
            const char *label =
                building.type == BuildingType::Base ? "B" : building.type == BuildingType::Factory ? "F" : "D";
            Art::DrawUiText(&art_, label, static_cast<int>(corner.x) + 6, static_cast<int>(corner.y) + 4, 24, WHITE);
        }
        else
        {
            art_.DrawBuilding(building.type, building.teamID, building.tileX, building.tileY);
        }
        if (building.isSelected)
        {
            const float w = static_cast<float>(size.x) * cc::TILE_SIZE;
            const float h = static_cast<float>(size.y) * cc::TILE_SIZE;
            DrawRectangleLinesEx({ corner.x, corner.y, w, h }, 3.0f, RED);
        }
        if (building.state == BuildingState::UnderConstruction)
        {
            const float w = static_cast<float>(size.x) * cc::TILE_SIZE;
            const float fraction = building.constructionTime / BuildingBuildTime(building.type);
            const float clamped = fraction < 0.0f ? 0.0f : (fraction > 1.0f ? 1.0f : fraction);
            DrawRectangle(static_cast<int>(corner.x), static_cast<int>(corner.y) - 10,
                          static_cast<int>(w), 6, LIGHTGRAY);
            DrawRectangle(static_cast<int>(corner.x), static_cast<int>(corner.y) - 10,
                          static_cast<int>(w * clamped), 6, DARKGREEN);
        }
    });
    if (playingInput_.PlacingType().has_value())
    {
        const cc::IVec2 tile = cc::WorldToTile(cc::ToGlm(input_.MouseWorld(camera_)));
        const cc::IVec2 fp = Footprint(*playingInput_.PlacingType());
        const Vector2 ghostCorner = cc::ToRaylib(cc::TileToWorld(tile.x, tile.y));
        const bool ok = CanPlaceBuilding(map_, &nodes_, *playingInput_.PlacingType(), tile.x, tile.y);
        DrawRectangleLinesEx({ ghostCorner.x, ghostCorner.y,
                               static_cast<float>(fp.x) * cc::TILE_SIZE,
                               static_cast<float>(fp.y) * cc::TILE_SIZE },
                             2.0f, ok ? GREEN : RED);
        Art::DrawUiText(&art_, TextFormat("Placing: %s (1/2/3 type, Z/Esc done)",
                            BuildingTypeName(*playingInput_.PlacingType())),
                 static_cast<int>(ghostCorner.x), static_cast<int>(ghostCorner.y) - 20, 14,
                 ok ? DARKGREEN : RED);
    }
    nodes_.Each([&](const ResourceNode &node) {
        if (!fog_.IsExplored(0, node.tile))
        {
            return;
        }
        const Vector2 corner = cc::ToRaylib(cc::TileToWorld(node.tile.x, node.tile.y));
        const Vector2 center = { corner.x + cc::TILE_SIZE / 2.0f,
                                 corner.y + cc::TILE_SIZE / 2.0f };
        if (art_.UseRectangles())
        {
            DrawCircleV(center, 14.0f, node.kind == ResourceKind::Iron ? GOLD : LIME);
        }
        else
        {
            art_.DrawNode(node.kind, center);
        }
        Art::DrawUiText(&art_, TextFormat("%.0f", node.amount), static_cast<int>(center.x) - 12,
                 static_cast<int>(center.y) - 8, 12, DARKGRAY);
    });

    registry_.Each<Unit>([&](Entity id, Unit &unit) {
        if (unit.teamID != 0 &&
            !fog_.IsVisible(0, cc::WorldToTile(cc::ToGlm(unit.position))))
        {
            return;
        }
        DEFINE_CHEAT_CODE(
            if (SHOW_UNIT_ID_TOGGLE.value())
            {
                Art::DrawUiText(&art_, TextFormat("%u", id),
                                static_cast<int>(unit.position.x),
                                static_cast<int>(unit.position.y), 12, YELLOW);
            }
        )
        const Rectangle body = { unit.position.x + 16.0f, unit.position.y + 16.0f, 32.0f, 32.0f };
        const Vector2 center = { body.x + 16.0f, body.y + 16.0f };
        if (art_.UseRectangles() && !art_.UseAtlas())
        {
            DrawRectangleRec(body, art_.TeamTint(unit.teamID));
            if (unit.state == UnitState::Attacking)
            {
                DrawRectangleLinesEx(body, 2.0f, ORANGE);
            }
        }
        else
        {
            std::array<Vector2, 6> slots;
            float slotScale = 1.0f;
            const int count = SquadSlots(unit.type, id, UnitHealthFraction(unit), slots, slotScale);
            const bool moving = unit.state == UnitState::Moving;
            const float animTime = static_cast<float>(GetTime());
            const Color tint = art_.TeamTint(unit.teamID);
            for (int i = 0; i < count; ++i)
            {
                const Vector2 corner = { unit.position.x + slots[i].x,
                                         unit.position.y + slots[i].y };
                if (art_.UseAtlas())
                {
                    const std::string sprite = art_.UnitSprite(unit.type, moving, id, animTime,
                                                            static_cast<int>(unit.facing));
                    if (!sprite.empty())
                    {
                        art_.DrawAtlasFrame(sprite, corner, tint,
                                           Art::BaseArtScale(unit.type));
                        continue;
                    }
                }
                if (!art_.UseRectangles())
                {
                    art_.DrawUnit(unit.type, unit.teamID, FrameForPhase(unit.phase), corner);
                }
                else
                {
                    DrawRectangleRec({ corner.x + 26.0f, corner.y + 26.0f, 12.0f, 12.0f },
                                     tint);
                }
            }
        }
        if (unit.isSelected)
        {
            const Rectangle selBox = SquadSelectionBox(
                art_, unit, id, !(art_.UseRectangles() && !art_.UseAtlas()), body);
            DrawRectangleLinesEx(selBox, 3.0f, RED);
            const float fraction = UnitHealthFraction(unit);
            const float barY = selBox.y - 7.0f;
            DrawRectangle(static_cast<int>(selBox.x), static_cast<int>(barY),
                          static_cast<int>(selBox.width), 5, Fade(RED, 0.6f));
            DrawRectangle(static_cast<int>(selBox.x), static_cast<int>(barY),
                          static_cast<int>(selBox.width * fraction), 5, GREEN);
            if (unit.attackRange > 0)
            {
                DrawCircleLinesV(center, static_cast<float>(unit.attackRange),
                                 Fade(RED, 0.35f));
            }
        }
        if (unit.controlGroups != 0)
        {
            int lowestBit = 0;
            while (lowestBit < 9 && (unit.controlGroups & (1u << lowestBit)) == 0)
            {
                ++lowestBit;
            }
            const Rectangle badgeBox = SquadSelectionBox(
                art_, unit, id, !(art_.UseRectangles() && !art_.UseAtlas()), body);
            Art::DrawUiText(&art_, TextFormat("%d", (lowestBit + 1) % 10), static_cast<int>(badgeBox.x),
                     static_cast<int>(badgeBox.y) - 14, 12, DARKBLUE);
        }
        if (unit.hitFlashTime > 0.0f)
        {
            DrawRectangleRec(body, Fade(WHITE, 0.7f));
        }
        if (unit.state == UnitState::Attacking)
        {
            if (const Unit *target = registry_.Get<Unit>(unit.target))
            {
                const Vector2 targetCenter = { target->position.x + 32.0f, target->position.y + 32.0f };
                DrawLineV(center, targetCenter, RED);
            }
        }
        if (unit.hasMoveOrder)
        {
            DrawCircleV(cc::ToRaylib(cc::ToGlm(unit.moveTarget) + cc::Vec2(32.0f, 32.0f)), 5.0f, GREEN);
        }
    });
    art_.ParticlesPool().Draw();
    damageNumbers_.Draw();
    for (int y = 0; y < map_.Height(); ++y)
    {
        for (int x = 0; x < map_.Width(); ++x)
        {
            const cc::IVec2 tile{ x, y };
            if (fog_.IsVisible(0, tile))
            {
                continue;
            }
            const Vector2 corner = cc::ToRaylib(cc::TileToWorld(x, y));
            const Color veil =
                fog_.IsExplored(0, tile) ? Fade(BLACK, 0.45f) : BLACK;
            DrawRectangleV(corner, { cc::TILE_SIZE, cc::TILE_SIZE }, veil);
        }
    }
    EndMode2D();
}

ConfirmChoice GameRenderer::DrawHudAndOverlays(int screenWidth, int screenHeight)
{
    // The modal owns input while open: lock raygui so HUD/overlay controls
    // behind it cannot fire.
    const bool modal = menu_.ConfirmOpen();
    if (modal)
    {
        GuiLock();
    }
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

    DrawResourcePanel(resources_, &art_);
    DrawSelectionPanel(registry_, &art_);
    DrawIdleButtons(registry_, 0);
    DrawRepairPanel(&playerAutoRepair_, &autoRepairCap_);
    DrawControlGroupStrip(registry_, 0, playingInput_.AutoAddGroupBit(),
                            &art_);
    DrawSaveSlots();
    sim_.RefreshFactory();
    DrawProductionPanel(resources_, queue_, sim_.HasFactory());
    if (playingInput_.IsSettingRally())
    {
        Art::DrawUiText(&art_, "Rally: left-click to place (R cancels)", 250, 364, 16, DARKGREEN);
    }
    if (playingInput_.AttackGroundMode())
    {
        Art::DrawUiText(&art_, "Shelling: right-click to fire (X/Esc cancels)", 250, 364, 16, RED);
    }
    if (!queue_.Empty())
    {
        const float ppw = 174.0f;
        const float ppx = static_cast<float>(screenWidth) - ppw - 10.0f;
        Art::DrawUiText(&art_, "Producing...", static_cast<int>(ppx), screenHeight - 222, 16, GRAY);
        DrawRectangle(static_cast<int>(ppx), screenHeight - 202, 150, 12, LIGHTGRAY);
        DrawRectangle(static_cast<int>(ppx), screenHeight - 202, static_cast<int>(150.0f * queue_.HeadProgress()), 12, DARKGREEN);
    }

    DrawFPS(screenWidth - 170, 135);
    Art::DrawUiText(&art_, TextFormat("Enemy: %s  Waves: %d", DifficultyName(worldDifficulty_),
                        ai_.WavesLaunched()),
             screenWidth - 170, 155, 16, GRAY);

    if (showHints_)
    {
        const std::vector<std::string> hints = ShortcutHintLines(hotkeys_);
        for (std::size_t i = 0; i < hints.size(); ++i)
        {
            Art::DrawUiText(&art_, hints[i].c_str(), 8, 250 + static_cast<int>(i) * 18, 14, Fade(DARKGRAY, 0.8f));
        }
    }

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
                    const std::vector<std::string> lines = UnitTooltipLines(*unit);
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

    if (menu_.state == MenuState::HotkeyRemap)
    {
        DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.5f));
        menuScreens_.DrawRemap(screenWidth / 2.0f);
    }
    if (menu_.state == MenuState::ReplayViewer)
    {
        Art::DrawUiText(&art_, TextFormat("Replay %d/%d", replayCursor_ + 1, sim_.ReplayCount()), 8, 96, 16,
                 DARKGRAY);
        Art::DrawUiText(&art_, "Left/Right step - Esc exit", 8, 116, 14, Fade(DARKGRAY, 0.8f));
    }
    if (menu_.state == MenuState::Paused)
    {
        DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.5f));
        if (GuiWindowBox(Rectangle{ 250, 40, 300, 400 }, "Paused"))
        {
            menu_.state = MenuState::Playing;
        }
        if (GuiButton(Rectangle{ 270, 85, 260, 30 }, "Resume"))
        {
            Announce(EventType::MenuAction);
            menu_.state = MenuState::Playing;
        }
        GuiLabel(Rectangle{ 270, 122, 260, 20 }, "Camera speed");
        GuiSlider(Rectangle{ 270, 145, 260, 20 }, "100", "800", &menu_.settings.cameraSpeed,
                  100.0f, 800.0f);
        GuiCheckBox(Rectangle{ 270, 170, 20, 20 }, "Minimap", &menu_.settings.showMinimap);
        GuiLabel(Rectangle{ 270, 195, 260, 20 }, "Master volume");
        GuiSlider(Rectangle{ 270, 218, 260, 20 }, "0", "1", &menu_.settings.masterVolume,
                  0.0f, 1.0f);
        GuiLabel(Rectangle{ 270, 243, 260, 20 }, "Music volume");
        GuiSlider(Rectangle{ 270, 266, 260, 20 }, "0", "1", &menu_.settings.musicVolume,
                  0.0f, 1.0f);
        GuiLabel(Rectangle{ 270, 291, 260, 20 }, "SFX volume");
        GuiSlider(Rectangle{ 270, 314, 260, 20 }, "0", "1", &menu_.settings.sfxVolume,
                  0.0f, 1.0f);
        GuiCheckBox(Rectangle{ 270, 340, 20, 20 }, "Mute", &menu_.settings.mute);
        GuiCheckBox(Rectangle{ 270, 365, 20, 20 }, "Right-drag pan",
                    &menu_.settings.rightDragPan);
        GuiCheckBox(Rectangle{ 270, 390, 20, 20 }, "Color-blind mode",
                    &menu_.settings.colorBlindMode);
        art_.SetColorBlindMode(menu_.settings.colorBlindMode);
        if (GuiButton(Rectangle{ 270, 415, 260, 30 }, "Remap hotkeys_..."))
        {
            menuScreens_.BeginRemap(MenuState::Paused);
        }
        if (GuiButton(Rectangle{ 270, 450, 260, 30 }, "Quit to menu_"))
        {
            quitToMenu_();
        }
        if (GuiButton(Rectangle{ 270, 485, 260, 30 }, "Quit to desktop"))
        {
            menu_.quitRequested = true;
        }
    }
    else if (menu_.state == MenuState::GameOver || menu_.state == MenuState::Victory)
    {
        DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.6f));
        const bool won = menu_.state == MenuState::Victory;
        if (GuiWindowBox(Rectangle{ 250, 140, 300, 195 }, won ? "Victory!" : "Defeat"))
        {
            menu_.quitRequested = true;
        }
        GuiLabel(Rectangle{ 270, 185, 260, 20 },
                 won ? "Enemy force destroyed." : "Your force was destroyed.");
        if (GuiButton(Rectangle{ 270, 240, 260, 30 }, "Return to menu_"))
        {
            quitToMenu_();
        }
        if (GuiButton(Rectangle{ 270, 280, 260, 30 }, "Quit to desktop"))
        {
            menu_.quitRequested = true;
        }
    }
    if (const float fade = MenuFadeAlpha(menuStateTime_); fade < 1.0f)
    {
        DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 1.0f - fade));
    }
    if (!modal)
    {
        return ConfirmChoice::None;
    }
    GuiUnlock();
    return DrawConfirmDialog(menu_, screenWidth, screenHeight);
}
