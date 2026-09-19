
#include "Game.h"

#include "raygui.h"
#include "Building.h"
#include "Cursor.h"
#include "DataRoot.h"
#include "Formation.h"
#include "Hud.h"
#include "Log.h"
#include "MapFile.h"
#include "Pathfinder.h"
#include "Selection.h"
#include "Shortcuts.h"
#include "Unit.h"
#include <algorithm>
#include <filesystem>
#include <vector>
#include <cmath>

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

Game::Game()
    : map(20, 15)
    , occ(20, 15)
    , factory(registry, resources, events)
    , ai(registry, map, nodes, events, 1, AIDifficulty::Medium, { 0, 0 }, { 0, 0 })
    , allyAI(registry, map, nodes, events, 0, AIDifficulty::Medium, { 0, 0 }, { 0, 0 })
    , enemyAI2(registry, map, nodes, events, 1, AIDifficulty::Medium, { 0, 0 }, { 0, 0 })
    , skirmish{ &registry, &resources, &map, &occ, &fog, &nodes,
                &queue,   &factory,   &ai, &allyAI, &enemyAI2, &camera, &rallyPos }
    , worldState{ &registry, &resources, &map, &camera, &nodes, &fog, &occ }
    , playingInput(registry, map, occ, nodes, camera, minimap, input, audio, menu.settings,
                   rallyPos)
    , sim(registry, map, occ, fog, nodes, queue, factory, resources, ai, allyAI, enemyAI2,
          art, audio, pings, menu, minimap, worldState, damageNumbers, events, rallyPos,
          playingInput.AutoAddGroupBit(), sandboxMode, worldIs2v2, playerAutoRepair,
          autoRepairCap, shakeTrauma, lastOutcomeState)
    , menuScreens(menu, art, audio, input, hotkeys, events,
                  MenuCallbacks{
                      [&](const std::string &mapPath, AIDifficulty difficulty) {
                          StartMatch(mapPath, difficulty);
                      },
                      [&](const std::string &slotPath) { LoadGameFromSlot(slotPath); },
                      [&]() { WatchLastReplay(); },
                      [&]() { BindShortcuts(); },
                  })
{
}

void Game::Init()
{
    if (const char *dataRoot =
            PickDataRoot(DirectoryExists("data"), GetApplicationDirectory(), DirectoryExists))
    {
        ChangeDirectory(dataRoot);
    }

    Log::Init("data/logs/conflict-converge.log");

    const int kInitialWidth = 1200;
    const int kInitialHeight = 675;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(kInitialWidth, kInitialHeight, "raylib basic window");
    SetExitKey(KEY_NULL); // Esc is handled per-screen, not by raylib
    SetWindowMinSize(800, 450);
    SetTargetFPS(60);
    GuiEnableTooltip();

    InitAudioDevice();
    audio.Init(IsAudioDeviceReady());
    art.Init(true);
    if (art.HasFont())
    {
        GuiSetFont(art.UiFont());
    }
    lastOutcomeState = MenuState::MainMenu;

    camera.view.offset = { kInitialWidth / 2.0f, kInitialHeight / 2.0f };
    camera.view.rotation = 0.0f;
    camera.view.zoom = 1.0f;
    LoadSettings(menu.settings, kSettingsPath);
    art.SetColorBlindMode(menu.settings.colorBlindMode);
    GuiSetStyle(DEFAULT, TEXT_SIZE, static_cast<int>(10 * menu.settings.uiScale));
    menuScreens.ApplyHotkeyOverrides();

    minimap.Init({ static_cast<float>(kInitialWidth) - 170.0f, 10.0f,
                   160.0f, 120.0f });

    worldActive = false;
    worldIs2v2 = false;
    worldDifficulty = AIDifficulty::Medium;
    worldMapPath.clear();
    showHints = true;

    events.Subscribe(EventType::UnitSpawned, [&](const Event &) { audio.Play(SfxId::Confirm); });
    events.Subscribe(EventType::UnitDestroyed, [&](const Event &e) {
        const auto *lifecycle = dynamic_cast<const UnitLifecycleEvent *>(&e);
        if (lifecycle != nullptr && lifecycle->teamID == 0)
        {
            pings.Raise({ lifecycle->position.x + 32.0f, lifecycle->position.y + 32.0f },
                        PingKind::UnitLost);
        }
    });

    BindShortcuts();
}

void Game::Announce(EventType type)
{
    Event bare;
    bare.type = type;
    events.Dispatch(bare);
}

MenuFlow &Game::E2EMenu()
{
    return menu;
}

Registry &Game::E2ERegistry()
{
    return registry;
}

TileMap &Game::E2EMap()
{
    return map;
}

bool Game::IsWorldActive() const
{
    return worldActive;
}

bool Game::E2EStartSelectedMatch()
{
    if (const MapEntry *sel = menu.setup.SelectedMap(); sel != nullptr)
    {
        if (menu.StartMatch())
        {
            StartMatch(sel->path, menu.setup.difficulty);
            return true;
        }
    }
    return false;
}

void Game::E2EQuitToMenu()
{
    QuitToMenu();
}

void Game::StartMatch(const std::string &mapPath, AIDifficulty difficulty)
{
    MapData startData;
    sandboxMode = ParseMapFile(mapPath, startData) && !startData.playerSpawns.empty() &&
                  startData.aiSpawns.empty();
    if (sandboxMode)
    {
        BuildSandbox(skirmish, mapPath);
    }
    else
    {
        BuildSkirmish(skirmish, mapPath, difficulty);
    }
    worldMapPath = mapPath;
    worldDifficulty = difficulty;
    worldActive = true;
    worldIs2v2 = !sandboxMode && SpotsForMap(mapPath).is2v2;
    playingInput.ResetForMatch();
    lastOutcomeState = MenuState::Playing;
    camera.view.zoom = 1.0f;
    minimap.elapsed = minimap.refreshInterval;
    sim.ResetForMatch();
    replayCursor = 0;
    replayPlayTimer = 0.0f;
    {
        std::error_code ec;
        std::filesystem::create_directories(kReplayDir, ec);
        for (int i = 0; i < kReplayMaxFrames; ++i)
        {
            std::filesystem::remove(ReplayFramePath(kReplayDir, i), ec);
        }
    }
    Announce(EventType::MenuAction);
    Announce(EventType::MatchStarted);
}

void Game::QuitToMenu()
{
    ResetSkirmish(skirmish);
    worldActive = false;
    worldIs2v2 = false;
    sandboxMode = false;
    sim.StopRecording();
    Announce(EventType::MenuAction);
    menu.OpenMainMenu();
}

void Game::PollConfirmKeys()
{
    if (!menu.ConfirmOpen())
    {
        return;
    }
    if (IsKeyPressed(KEY_Y))
    {
        pendingConfirm = ConfirmChoice::Yes;
        return;
    }
    const int backKey = hotkeys.KeyFor("Back");
    if (IsKeyPressed(KEY_N) || (backKey != 0 && IsKeyPressed(backKey)))
    {
        pendingConfirm = ConfirmChoice::No;
    }
}

void Game::ApplyConfirmChoice(ConfirmChoice choice)
{
    if (choice == ConfirmChoice::None)
    {
        return;
    }
    const ConfirmKind kind = menu.confirm;
    menu.CloseConfirm();
    if (choice == ConfirmChoice::Yes)
    {
        if (kind == ConfirmKind::QuitApp)
        {
            menu.quitRequested = true;
        }
        else if (kind == ConfirmKind::BackToMenu)
        {
            QuitToMenu();
        }
    }
}

void Game::LoadGameFromSlot(const std::string &slotPath)
{
    ResetSkirmish(skirmish);
    if (!LoadWorld(worldState, slotPath))
    {
        return;
    }
    const SkirmishSpots spots =
        SpotsForMap(worldMapPath.empty() ? "data/maps/crossroads.map" : worldMapPath);
    ai.Reset(menu.setup.difficulty, spots.aiHome, spots.playerHome, 1);
    if (spots.is2v2)
    {
        allyAI.Reset(menu.setup.difficulty, spots.allyHome, spots.aiHome, 0);
        enemyAI2.Reset(menu.setup.difficulty, spots.enemyHome2, spots.playerHome, 1);
    }
    rallyPos = camera.view.target;
    worldDifficulty = menu.setup.difficulty;
    worldActive = true;
    worldIs2v2 = spots.is2v2;
    playingInput.ResetForMatch();
    sim.ResetEdgePolls();
    lastOutcomeState = MenuState::Playing;
    camera.view.zoom = 1.0f;
    minimap.elapsed = minimap.refreshInterval;
    menu.state = MenuState::Playing;
    Announce(EventType::MenuAction);
    Announce(EventType::MatchStarted);
}

void Game::StepReplay(int dir)
{
    if (sim.ReplayCount() <= 0)
    {
        return;
    }
    replayCursor += dir;
    if (replayCursor < 0)
    {
        replayCursor = 0;
    }
    if (replayCursor >= sim.ReplayCount())
    {
        replayCursor = sim.ReplayCount() - 1;
    }
    if (LoadWorld(worldState, ReplayFramePath(kReplayDir, replayCursor)))
    {
        replayPlayTimer = 0.0f;
        minimap.elapsed = minimap.refreshInterval;
    }
}

bool Game::WatchLastReplay()
{
    const int count = ReplayFrameCount(kReplayDir);
    if (count <= 0)
    {
        return false;
    }
    ResetSkirmish(skirmish);
    if (!LoadWorld(worldState, ReplayFramePath(kReplayDir, 0)))
    {
        return false;
    }
    sim.StopRecording();
    sim.SetReplayCount(count);
    replayCursor = 0;
    replayPlayTimer = 0.0f;
    worldActive = true;
    worldIs2v2 = false;
    playingInput.ResetForMatch();
    camera.view.zoom = 1.0f;
    minimap.elapsed = minimap.refreshInterval;
    menu.state = MenuState::ReplayViewer;
    Announce(EventType::MenuAction);
    return true;
}

void Game::BindShortcuts()
{
    input.shortcuts.Clear();
    input.shortcuts.Bind(hotkeys.KeyFor("ToggleHints"), [&] { showHints = !showHints; });
    input.shortcuts.Bind(hotkeys.KeyFor("TogglePause"), [&] {
        if (menu.state == MenuState::Playing)
        {
            menu.TogglePause();
            Announce(EventType::MenuAction);
            Announce(EventType::MatchPaused);
        }
        else if (menu.state == MenuState::Paused)
        {
            menu.TogglePause();
            Announce(EventType::MenuAction);
        }
    });
    input.shortcuts.Bind(hotkeys.KeyFor("Quicksave"), [&] {
        if (!worldActive || menu.state != MenuState::Playing)
        {
            return;
        }
        SaveWorld({ &registry, &resources, &map, &camera, &nodes, &fog, &occ }, "data/quicksave.ccpb");
    });
    input.shortcuts.Bind(hotkeys.KeyFor("Quickload"), [&] {
        if (!worldActive || menu.state != MenuState::Playing)
        {
            return;
        }
        LoadWorld({ &registry, &resources, &map, &camera, &nodes, &fog, &occ }, "data/quicksave.ccpb");
    });
    input.shortcuts.Bind(hotkeys.KeyFor("SaveSlot1"), [&] {
        if (worldActive && menu.state == MenuState::Playing)
        {
            SaveWorld(worldState, SaveSlotPath(1));
        }
    });
    input.shortcuts.Bind(hotkeys.KeyFor("SaveSlot2"), [&] {
        if (worldActive && menu.state == MenuState::Playing)
        {
            SaveWorld(worldState, SaveSlotPath(2));
        }
    });
    input.shortcuts.Bind(hotkeys.KeyFor("SaveSlot3"), [&] {
        if (worldActive && menu.state == MenuState::Playing)
        {
            SaveWorld(worldState, SaveSlotPath(3));
        }
    });
    input.shortcuts.BindChord(hotkeys.KeyFor("LoadSlot1"), [&] {
        if (worldActive && menu.state == MenuState::Playing)
        {
            LoadWorld(worldState, SaveSlotPath(1));
        }
    });
    input.shortcuts.BindChord(hotkeys.KeyFor("LoadSlot2"), [&] {
        if (worldActive && menu.state == MenuState::Playing)
        {
            LoadWorld(worldState, SaveSlotPath(2));
        }
    });
    input.shortcuts.BindChord(hotkeys.KeyFor("LoadSlot3"), [&] {
        if (worldActive && menu.state == MenuState::Playing)
        {
            LoadWorld(worldState, SaveSlotPath(3));
        }
    });
    input.shortcuts.Bind(hotkeys.KeyFor("AttackMove"), [&] {
        if (!worldActive || menu.state != MenuState::Playing)
        {
            return;
        }
        const Vector2 dest = input.MouseWorld(camera);
        const bool queued = input.ShiftDown();
        int acted = 0;
        registry.Each<Unit>([&](Entity id, Unit &unit) {
            if (unit.isSelected)
            {
                IssueOrEnqueue(unit, map, &occ, id, registry.Generation(id), queued,
                               QueuedOrder{ QueuedOrderKind::AttackMove, dest });
                ++acted;
            }
        });
        if (acted > 0)
        {
            audio.Play(SfxId::Confirm);
        }
    });
    input.shortcuts.Bind(hotkeys.KeyFor("StanceHold"), [&] {
        if (!worldActive || menu.state != MenuState::Playing)
        {
            return;
        }
        const Entity selected = SelectedUnit(registry);
        if (Unit *unit = registry.Get<Unit>(selected))
        {
            SetStance(*unit, Stance::Hold);
        }
    });
    input.shortcuts.Bind(hotkeys.KeyFor("StanceGuard"), [&] {
        if (!worldActive || menu.state != MenuState::Playing)
        {
            return;
        }
        const Entity selected = SelectedUnit(registry);
        if (Unit *unit = registry.Get<Unit>(selected))
        {
            SetStance(*unit, Stance::Guard);
        }
    });
    input.shortcuts.Bind(hotkeys.KeyFor("Patrol"), [&] {
        if (!worldActive || menu.state != MenuState::Playing)
        {
            return;
        }
        const Vector2 dest = input.MouseWorld(camera);
        const bool queued = input.ShiftDown();
        int acted = 0;
        registry.Each<Unit>([&](Entity id, Unit &unit) {
            if (unit.isSelected)
            {
                IssueOrEnqueue(unit, map, &occ, id, registry.Generation(id), queued,
                               QueuedOrder{ QueuedOrderKind::Patrol, unit.position, dest });
                ++acted;
            }
        });
        if (acted > 0)
        {
            audio.Play(SfxId::Confirm);
        }
    });
    input.shortcuts.Bind(hotkeys.KeyFor("Rally"), [&] {
        if (worldActive && menu.state == MenuState::Playing)
        {
            playingInput.ToggleSettingRally();
        }
    });
    input.shortcuts.Bind(hotkeys.KeyFor("SelectType"), [&] {
        if (!worldActive || menu.state != MenuState::Playing)
        {
            return;
        }
        const Entity selected = SelectedUnit(registry);
        const Unit *unit = registry.Get<Unit>(selected);
        if (unit != nullptr)
        {
            SelectAllOfType(registry, unit->type, 0, false);
        }
    });
    input.shortcuts.Bind(hotkeys.KeyFor("SelectFactories"), [&] {
        if (!worldActive || menu.state != MenuState::Playing)
        {
            return;
        }
        SelectAllBuildings(registry, BuildingType::Factory, 0);
    });
    input.shortcuts.Bind(hotkeys.KeyFor("SlowestSpeed"), [&] {
        if (worldActive && menu.state == MenuState::Playing)
        {
            playingInput.ToggleSlowestSpeed();
        }
    });
    input.shortcuts.Bind(hotkeys.KeyFor("AreaBuild"), [&] {
        if (worldActive && menu.state == MenuState::Playing)
        {
            playingInput.ToggleAreaBuild();
        }
    });
    input.shortcuts.Bind(hotkeys.KeyFor("AreaRepair"), [&] {
        if (worldActive && menu.state == MenuState::Playing)
        {
            playingInput.ToggleAreaRepair();
        }
    });
    input.shortcuts.Bind(hotkeys.KeyFor("AttackGround"), [&] {
        if (worldActive && menu.state == MenuState::Playing)
        {
            playingInput.ToggleAttackGround();
        }
    });
    input.shortcuts.Bind(hotkeys.KeyFor("AutoRetreat"), [&] {
        if (!worldActive || menu.state != MenuState::Playing)
        {
            return;
        }
        bool allOn = true;
        registry.Each<Unit>([&](Entity, const Unit &unit) {
            if (unit.isSelected && !unit.autoRetreat)
            {
                allOn = false;
            }
        });
        registry.Each<Unit>([&](Entity, Unit &unit) {
            if (unit.isSelected)
            {
                unit.autoRetreat = !allOn;
            }
        });
    });
    input.shortcuts.Bind(hotkeys.KeyFor("JumpPing"), [&] {
        if (!worldActive || menu.state != MenuState::Playing)
        {
            return;
        }
        Vector2 pingPos = {};
        if (pings.Latest(pingPos))
        {
            camera.view.target = pingPos;
        }
    });
    input.shortcuts.Bind(hotkeys.KeyFor("ReplayBack"), [&] {
        if (menu.state == MenuState::ReplayViewer)
        {
            StepReplay(-1);
        }
    });
    input.shortcuts.Bind(hotkeys.KeyFor("ReplayFwd"), [&] {
        if (menu.state == MenuState::ReplayViewer)
        {
            StepReplay(1);
        }
    });
    input.shortcuts.Bind(hotkeys.KeyFor("Back"), [&] {
        if (menuScreens.CancelRemapCapture())
        {
            return;
        }
        switch (menu.OnBackPressed())
        {
        case BackAction::Navigate:
        case BackAction::OpenQuitConfirm:
        case BackAction::OpenBackToMenuConfirm:
            Announce(EventType::MenuAction);
            break;
        case BackAction::QuitToMenu:
            QuitToMenu();
            break;
        case BackAction::None:
        default:
            break;
        }
    });
    input.shortcuts.Bind(hotkeys.KeyFor("Halt"), [&] {
        if (!worldActive || menu.state != MenuState::Playing)
        {
            return;
        }
        registry.Each<Unit>([&](Entity, Unit &unit) {
            if (unit.isSelected)
            {
                unit.hasMoveOrder = false;
                unit.hasPath = false;
                unit.path.clear();
                unit.pathNext = 0;
                unit.target = kInvalidEntity;
                unit.attackMove = false;
                unit.hasRepairOrder = false;
                unit.repairTarget = kInvalidEntity;
                unit.hasAttackGroundOrder = false;
                unit.speedCapPixelsPerSec = -1.0f;
                unit.orderQueue.clear();
                unit.phase = AttackPhase::Ready;
                unit.velocity = { 0.0f, 0.0f };
                unit.state = UnitState::Idle;
                SnapUnitToTile(unit);
            }
        });
    });
}

void Game::DrawWorld()
{
    {
        CursorIntent intent = CursorIntent::Default;
        if (playingInput.PlacingType().has_value())
        {
            const cc::IVec2 tile = cc::WorldToTile(cc::ToGlm(input.MouseWorld(camera)));
            intent = CanPlaceBuilding(map, &nodes, *playingInput.PlacingType(), tile.x, tile.y)
                         ? CursorIntent::Default
                         : CursorIntent::InvalidPlacement;
        }
        else if (playingInput.AttackGroundMode())
        {
            intent = CursorIntent::Attack;
        }
        else
        {
            const Entity selectedId = SelectedUnit(registry);
            const Unit *selected =
                selectedId != kInvalidEntity ? registry.Get<Unit>(selectedId) : nullptr;
            intent = PredictCursorIntent(registry, selected, input.MouseWorld(camera));
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

    Camera2D shaken = camera.view;
    if (shakeTrauma > 0.0f)
    {
        const float amount = ShakeMagnitude(shakeTrauma);
        shaken.offset.x += static_cast<float>(GetRandomValue(-1000, 1000)) / 1000.0f * amount;
        shaken.offset.y += static_cast<float>(GetRandomValue(-1000, 1000)) / 1000.0f * amount;
    }
    BeginMode2D(shaken);

    for (int y = 0; y < map.Height(); ++y)
    {
        for (int x = 0; x < map.Width(); ++x)
        {
            const Vector2 corner = cc::ToRaylib(cc::TileToWorld(x, y));
            const TerrainType terrain = map.Get({ x, y });
            if (art.UseRectangles())
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
                art.DrawTerrain(terrain, corner);
            }
        }
    }

    registry.Each<Building>([&](Entity, const Building &building) {
        if (building.teamID != 0 &&
            !fog.IsVisible(0, cc::IVec2{ building.tileX, building.tileY }))
        {
            return;
        }
        const cc::IVec2 size = Footprint(building.type);
        const Vector2 corner = cc::ToRaylib(cc::TileToWorld(building.tileX, building.tileY));
        if (art.UseRectangles())
        {
            const float w = static_cast<float>(size.x) * cc::TILE_SIZE;
            const float h = static_cast<float>(size.y) * cc::TILE_SIZE;
            const Color tint =
                building.type == BuildingType::Base ? DARKGRAY : building.type == BuildingType::Factory ? BROWN : GRAY;
            DrawRectangleV(corner, { w, h }, tint);
            const char *label =
                building.type == BuildingType::Base ? "B" : building.type == BuildingType::Factory ? "F" : "D";
            Art::DrawUiText(&art, label, static_cast<int>(corner.x) + 6, static_cast<int>(corner.y) + 4, 24, WHITE);
        }
        else
        {
            art.DrawBuilding(building.type, building.teamID, building.tileX, building.tileY);
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
    if (playingInput.PlacingType().has_value())
    {
        const cc::IVec2 tile = cc::WorldToTile(cc::ToGlm(input.MouseWorld(camera)));
        const cc::IVec2 fp = Footprint(*playingInput.PlacingType());
        const Vector2 ghostCorner = cc::ToRaylib(cc::TileToWorld(tile.x, tile.y));
        const bool ok = CanPlaceBuilding(map, &nodes, *playingInput.PlacingType(), tile.x, tile.y);
        DrawRectangleLinesEx({ ghostCorner.x, ghostCorner.y,
                               static_cast<float>(fp.x) * cc::TILE_SIZE,
                               static_cast<float>(fp.y) * cc::TILE_SIZE },
                             2.0f, ok ? GREEN : RED);
        Art::DrawUiText(&art, TextFormat("Placing: %s (1/2/3 type, Z/Esc done)",
                            BuildingTypeName(*playingInput.PlacingType())),
                 static_cast<int>(ghostCorner.x), static_cast<int>(ghostCorner.y) - 20, 14,
                 ok ? DARKGREEN : RED);
    }
    nodes.Each([&](const ResourceNode &node) {
        if (!fog.IsExplored(0, node.tile))
        {
            return;
        }
        const Vector2 corner = cc::ToRaylib(cc::TileToWorld(node.tile.x, node.tile.y));
        const Vector2 center = { corner.x + cc::TILE_SIZE / 2.0f,
                                 corner.y + cc::TILE_SIZE / 2.0f };
        if (art.UseRectangles())
        {
            DrawCircleV(center, 14.0f, node.kind == ResourceKind::Iron ? GOLD : LIME);
        }
        else
        {
            art.DrawNode(node.kind, center);
        }
        Art::DrawUiText(&art, TextFormat("%.0f", node.amount), static_cast<int>(center.x) - 12,
                 static_cast<int>(center.y) - 8, 12, DARKGRAY);
    });

    registry.Each<Unit>([&](Entity id, Unit &unit) {
        if (unit.teamID != 0 &&
            !fog.IsVisible(0, cc::WorldToTile(cc::ToGlm(unit.position))))
        {
            return;
        }
        const Rectangle body = { unit.position.x + 16.0f, unit.position.y + 16.0f, 32.0f, 32.0f };
        const Vector2 center = { body.x + 16.0f, body.y + 16.0f };
        if (art.UseRectangles() && !art.UseAtlas())
        {
            DrawRectangleRec(body, art.TeamTint(unit.teamID));
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
            const Color tint = art.TeamTint(unit.teamID);
            for (int i = 0; i < count; ++i)
            {
                const Vector2 corner = { unit.position.x + slots[i].x,
                                         unit.position.y + slots[i].y };
                if (art.UseAtlas())
                {
                    const std::string sprite = art.UnitSprite(unit.type, moving, id, animTime,
                                                            static_cast<int>(unit.facing));
                    if (!sprite.empty())
                    {
                        art.DrawAtlasFrame(sprite, corner, tint,
                                           Art::BaseArtScale(unit.type));
                        continue;
                    }
                }
                if (!art.UseRectangles())
                {
                    art.DrawUnit(unit.type, unit.teamID, FrameForPhase(unit.phase), corner);
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
                art, unit, id, !(art.UseRectangles() && !art.UseAtlas()), body);
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
                art, unit, id, !(art.UseRectangles() && !art.UseAtlas()), body);
            Art::DrawUiText(&art, TextFormat("%d", (lowestBit + 1) % 10), static_cast<int>(badgeBox.x),
                     static_cast<int>(badgeBox.y) - 14, 12, DARKBLUE);
        }
        if (unit.hitFlashTime > 0.0f)
        {
            DrawRectangleRec(body, Fade(WHITE, 0.7f));
        }
        if (unit.state == UnitState::Attacking)
        {
            if (const Unit *target = registry.Get<Unit>(unit.target))
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
    art.ParticlesPool().Draw();
    damageNumbers.Draw();
    for (int y = 0; y < map.Height(); ++y)
    {
        for (int x = 0; x < map.Width(); ++x)
        {
            const cc::IVec2 tile{ x, y };
            if (fog.IsVisible(0, tile))
            {
                continue;
            }
            const Vector2 corner = cc::ToRaylib(cc::TileToWorld(x, y));
            const Color veil =
                fog.IsExplored(0, tile) ? Fade(BLACK, 0.45f) : BLACK;
            DrawRectangleV(corner, { cc::TILE_SIZE, cc::TILE_SIZE }, veil);
        }
    }
    EndMode2D();
}

void Game::DrawHudAndOverlays(int screenWidth, int screenHeight)
{
    // The modal owns input while open: lock raygui so HUD/overlay controls
    // behind it cannot fire.
    const bool modal = menu.ConfirmOpen();
    if (modal)
    {
        GuiLock();
    }
    if (playingInput.IsDragging() && input.LeftDown())
    {
        DrawRectangleLinesEx(
            NormalizeRect(playingInput.DragStart(), input.MouseScreen()), 1.0f, GREEN);
    }
    if (playingInput.RepairDragActive() && input.LeftDown() && playingInput.AreaRepairMode())
    {
        DrawRectangleLinesEx(
            NormalizeRect(playingInput.RepairDragStart(), input.MouseScreen()), 1.0f, GREEN);
        std::vector<Entity> preview;
        CollectAreaRepairCandidates(
            registry,
            DraggedWorldBox(camera, playingInput.RepairDragStart(), input.MouseScreen()), 0,
            preview);
        for (const Entity id : preview)
        {
            Vector2 center = { 0.0f, 0.0f };
            if (const Unit *unit = registry.Get<Unit>(id))
            {
                center = { unit->position.x + 32.0f, unit->position.y + 32.0f };
            }
            else if (const Building *building = registry.Get<Building>(id))
            {
                const Rectangle fp = BuildingFootprintRect(*building);
                center = { fp.x + fp.width / 2.0f, fp.y + fp.height / 2.0f };
            }
            DrawCircleV(GetWorldToScreen2D(center, camera.view), 4.0f, GREEN);
        }
    }
    if (playingInput.IsRightDragging() && input.RightDown())
    {
        DrawLineEx(playingInput.RightDragStart(), input.MouseScreen(), 2.0f, SKYBLUE);
        DrawCircleV(playingInput.RightDragStart(), 3.0f, SKYBLUE);
        DrawCircleV(input.MouseScreen(), 3.0f, SKYBLUE);
    }

    if (menu.settings.showMinimap)
    {
        DrawTextureRec(minimap.target.texture,
                       { 0.0f, 0.0f, minimap.screenRect.width, -minimap.screenRect.height },
                       { minimap.screenRect.x, minimap.screenRect.y }, WHITE);
        DrawRectangleLinesEx(
            minimap.ViewportRect(camera.view, screenWidth, screenHeight, map.Width(), map.Height()),
            1.0f, WHITE);
        DrawRectangleLinesEx(minimap.screenRect, 1.0f, DARKGRAY);
        for (const Ping &ping : pings.Active())
        {
            const Vector2 blip = minimap.WorldToMinimap(ping.worldPos, map.Width(), map.Height());
            const float pulse = 2.0f + ping.age * 2.0f;
            const Color color = ping.kind == PingKind::UnderAttack
                                    ? Fade(RED, 1.0f - ping.age / 5.0f)
                                    : Fade(ORANGE, 1.0f - ping.age / 5.0f);
            DrawCircleV(blip, pulse, color);
        }
    }

    DrawResourcePanel(resources, &art);
    DrawSelectionPanel(registry, &art);
    DrawIdleButtons(registry, 0);
    DrawRepairPanel(&playerAutoRepair, &autoRepairCap);
    DrawControlGroupStrip(registry, 0, playingInput.AutoAddGroupBit(),
                            &art);
    DrawSaveSlots();
    sim.RefreshFactory();
    DrawProductionPanel(resources, queue, sim.HasFactory());
    if (playingInput.IsSettingRally())
    {
        Art::DrawUiText(&art, "Rally: left-click to place (R cancels)", 250, 364, 16, DARKGREEN);
    }
    if (playingInput.AttackGroundMode())
    {
        Art::DrawUiText(&art, "Shelling: right-click to fire (X/Esc cancels)", 250, 364, 16, RED);
    }
    if (!queue.Empty())
    {
        const float ppw = 174.0f;
        const float ppx = static_cast<float>(screenWidth) - ppw - 10.0f;
        Art::DrawUiText(&art, "Producing...", static_cast<int>(ppx), screenHeight - 222, 16, GRAY);
        DrawRectangle(static_cast<int>(ppx), screenHeight - 202, 150, 12, LIGHTGRAY);
        DrawRectangle(static_cast<int>(ppx), screenHeight - 202, static_cast<int>(150.0f * queue.HeadProgress()), 12, DARKGREEN);
    }

    DrawFPS(screenWidth - 170, 135);
    Art::DrawUiText(&art, TextFormat("Enemy: %s  Waves: %d", DifficultyName(worldDifficulty),
                        ai.WavesLaunched()),
             screenWidth - 170, 155, 16, GRAY);

    if (showHints)
    {
        const std::vector<std::string> hints = ShortcutHintLines(hotkeys);
        for (std::size_t i = 0; i < hints.size(); ++i)
        {
            Art::DrawUiText(&art, hints[i].c_str(), 8, 250 + static_cast<int>(i) * 18, 14, Fade(DARKGRAY, 0.8f));
        }
    }

    if (menu.state == MenuState::Playing && !playingInput.IsDragging() &&
        !playingInput.PlaceDragActive() && !playingInput.RepairDragActive() &&
        !playingInput.IsRightDragging() && !playingInput.PlacingType().has_value() &&
        !playingInput.AttackGroundMode() && !playingInput.AreaRepairMode())
    {
        const Entity hovered = PickUnitAt(registry, input.MouseWorld(camera));
        if (UpdateHoverTooltip(hoverTip, hovered, GetFrameTime(), kHoverTooltipDelay))
        {
            if (const Unit *unit = registry.Get<Unit>(hoverTip.hovered))
            {
                const bool visible =
                    unit->teamID == 0 ||
                    fog.IsVisible(0, cc::WorldToTile(cc::ToGlm(unit->position)));
                if (visible)
                {
                    const std::vector<std::string> lines = UnitTooltipLines(*unit);
                    int width = 0;
                    for (const std::string &line : lines)
                    {
                        width = std::max(width, MeasureText(line.c_str(), 14));
                    }
                    const Vector2 mouse = input.MouseScreen();
                    const int tx = static_cast<int>(mouse.x) + 16;
                    const int ty = static_cast<int>(mouse.y) + 20;
                    DrawRectangle(tx - 4, ty - 4, width + 8,
                                  static_cast<int>(lines.size()) * 18 + 4, Fade(BLACK, 0.75f));
                    for (std::size_t i = 0; i < lines.size(); ++i)
                    {
                        Art::DrawUiText(&art, lines[i].c_str(), tx, ty + static_cast<int>(i) * 18, 14,
                                 RAYWHITE);
                    }
                }
            }
        }
    }
    else
    {
        hoverTip.hovered = kInvalidEntity;
        hoverTip.time = 0.0f;
    }

    if (menu.state == MenuState::HotkeyRemap)
    {
        DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.5f));
        menuScreens.DrawRemap(screenWidth / 2.0f);
    }
    if (menu.state == MenuState::ReplayViewer)
    {
        Art::DrawUiText(&art, TextFormat("Replay %d/%d", replayCursor + 1, sim.ReplayCount()), 8, 96, 16,
                 DARKGRAY);
        Art::DrawUiText(&art, "Left/Right step - Esc exit", 8, 116, 14, Fade(DARKGRAY, 0.8f));
    }
    if (menu.state == MenuState::Paused)
    {
        DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.5f));
        if (GuiWindowBox(Rectangle{ 250, 40, 300, 400 }, "Paused"))
        {
            menu.state = MenuState::Playing;
        }
        if (GuiButton(Rectangle{ 270, 85, 260, 30 }, "Resume"))
        {
            Announce(EventType::MenuAction);
            menu.state = MenuState::Playing;
        }
        GuiLabel(Rectangle{ 270, 122, 260, 20 }, "Camera speed");
        GuiSlider(Rectangle{ 270, 145, 260, 20 }, "100", "800", &menu.settings.cameraSpeed,
                  100.0f, 800.0f);
        GuiCheckBox(Rectangle{ 270, 170, 20, 20 }, "Minimap", &menu.settings.showMinimap);
        GuiLabel(Rectangle{ 270, 195, 260, 20 }, "Master volume");
        GuiSlider(Rectangle{ 270, 218, 260, 20 }, "0", "1", &menu.settings.masterVolume,
                  0.0f, 1.0f);
        GuiLabel(Rectangle{ 270, 243, 260, 20 }, "Music volume");
        GuiSlider(Rectangle{ 270, 266, 260, 20 }, "0", "1", &menu.settings.musicVolume,
                  0.0f, 1.0f);
        GuiLabel(Rectangle{ 270, 291, 260, 20 }, "SFX volume");
        GuiSlider(Rectangle{ 270, 314, 260, 20 }, "0", "1", &menu.settings.sfxVolume,
                  0.0f, 1.0f);
        GuiCheckBox(Rectangle{ 270, 340, 20, 20 }, "Mute", &menu.settings.mute);
        GuiCheckBox(Rectangle{ 270, 365, 20, 20 }, "Right-drag pan",
                    &menu.settings.rightDragPan);
        GuiCheckBox(Rectangle{ 270, 390, 20, 20 }, "Color-blind mode",
                    &menu.settings.colorBlindMode);
        art.SetColorBlindMode(menu.settings.colorBlindMode);
        if (GuiButton(Rectangle{ 270, 415, 260, 30 }, "Remap hotkeys..."))
        {
            menuScreens.BeginRemap(MenuState::Paused);
        }
        if (GuiButton(Rectangle{ 270, 450, 260, 30 }, "Quit to menu"))
        {
            QuitToMenu();
        }
        if (GuiButton(Rectangle{ 270, 485, 260, 30 }, "Quit to desktop"))
        {
            menu.quitRequested = true;
        }
    }
    else if (menu.state == MenuState::GameOver || menu.state == MenuState::Victory)
    {
        DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.6f));
        const bool won = menu.state == MenuState::Victory;
        if (GuiWindowBox(Rectangle{ 250, 140, 300, 195 }, won ? "Victory!" : "Defeat"))
        {
            menu.quitRequested = true;
        }
        GuiLabel(Rectangle{ 270, 185, 260, 20 },
                 won ? "Enemy force destroyed." : "Your force was destroyed.");
        if (GuiButton(Rectangle{ 270, 240, 260, 30 }, "Return to menu"))
        {
            QuitToMenu();
        }
        if (GuiButton(Rectangle{ 270, 280, 260, 30 }, "Quit to desktop"))
        {
            menu.quitRequested = true;
        }
    }
    if (const float fade = MenuFadeAlpha(menuStateTime); fade < 1.0f)
    {
        DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 1.0f - fade));
    }
    if (!modal)
    {
        return;
    }
    GuiUnlock();
    if (const ConfirmChoice choice = DrawConfirmDialog(menu, screenWidth, screenHeight);
        choice != ConfirmChoice::None)
    {
        pendingConfirm = choice;
    }
}

void Game::Update()
{
    if (pendingConfirm != ConfirmChoice::None)
    {
        ApplyConfirmChoice(pendingConfirm);
        pendingConfirm = ConfirmChoice::None;
    }
    PollConfirmKeys();
    input.shortcuts.SetEnabled(!menu.ConfirmOpen());
    input.Update(camera, menu.settings.cameraSpeed, GetFrameTime());
    camera.AdjustZoom(input.WheelDelta());
    const int screenWidth = GetScreenWidth();
    const int screenHeight = GetScreenHeight();
    camera.view.offset = { screenWidth / 2.0f, screenHeight / 2.0f };
    minimap.screenRect.x = static_cast<float>(screenWidth) - minimap.screenRect.width - 10.0f;
    minimap.screenRect.y = 10.0f;
    camera.ClampZoomToWorld(static_cast<float>(map.Width()) * cc::TILE_SIZE,
                            static_cast<float>(map.Height()) * cc::TILE_SIZE, screenWidth,
                            screenHeight);
    camera.ClampToMap(static_cast<float>(map.Width()) * cc::TILE_SIZE,
                      static_cast<float>(map.Height()) * cc::TILE_SIZE, screenWidth,
                      screenHeight);
    TrackMenuTransition(previousMenuState, menuStateTime, menu.state, GetFrameTime());

    if (!worldActive)
    {
        if (const ConfirmChoice choice = menuScreens.Draw(screenWidth, screenHeight, menuStateTime);
            choice != ConfirmChoice::None)
        {
            pendingConfirm = choice;
        }
        return;
    }

    if (menu.state == MenuState::Playing && !menu.ConfirmOpen())
    {
        playingInput.Dispatch();
        sim.Step(GetFrameTime());
    }

    audio.ApplySettings(menu.settings.masterVolume, menu.settings.musicVolume,
                        menu.settings.sfxVolume, menu.settings.mute);
    audio.UpdateMusic();

    if (menu.state == MenuState::ReplayViewer && sim.ReplayCount() > 0)
    {
        replayPlayTimer += GetFrameTime();
        if (replayPlayTimer >= 2.0f)
        {
            replayPlayTimer = 0.0f;
            if (replayCursor + 1 < sim.ReplayCount())
            {
                StepReplay(1);
            }
        }
    }
    pings.Update(GetFrameTime());
    shakeTrauma = DecayShakeTrauma(shakeTrauma, GetFrameTime());

    BeginDrawing();
    ClearBackground(RAYWHITE);

    DrawWorld();

    DrawHudAndOverlays(screenWidth, screenHeight);
    EndDrawing();
}

void Game::Shutdown()
{
    minimap.Unload();
    art.Shutdown();
    audio.Shutdown();
    CloseAudioDevice();
    CloseWindow();
    Log::Shutdown();
}

bool Game::IsRunning() const
{
    return !WindowShouldClose() && !menu.quitRequested;
}

int Game::Run()
{
    Init();
    while (IsRunning())
    {
        Update();
    }
    Shutdown();
    return 0;
}
