
#include "Game.h"

#include "raygui.h"
#include "DataRoot.h"
#include "Log.h"

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
                      [&](const std::string &slotPath) { match.LoadGameFromSlot(slotPath); },
                      [&]() { match.WatchLastReplay(); },
                      [&]() { bindings.Bind(); },
                  })
    , bindings(input, hotkeys, menu, menuScreens, playingInput, audio, camera, map, occ,
                nodes, fog, registry, resources, events, worldState, pings, worldActive,
                showHints, [this]() { QuitToMenu(); }, [this](int dir) { match.StepReplay(dir); })
    , rmlUiMenus(menu, hotkeys, art, audio, events, menuScreens,
                 MenuCallbacks{
                     [&](const std::string &mapPath, AIDifficulty difficulty) {
                         StartMatch(mapPath, difficulty);
                     },
                     [&](const std::string &slotPath) { match.LoadGameFromSlot(slotPath); },
                     [&]() { match.WatchLastReplay(); },
                     [&]() { bindings.Bind(); },
                 })
    , renderer(art, camera, map, registry, fog, nodes, minimap, pings, damageNumbers,
               playingInput, menu, sim, resources, queue, hotkeys, input, ai, menuScreens,
               events, showHints, replayCursor, worldDifficulty, menuStateTime, shakeTrauma,
               playerAutoRepair, autoRepairCap, rmlUi, [this]() { QuitToMenu(); })
    , match(camera, ai, allyAI, enemyAI2, menu, minimap, playingInput, sim, events,
            skirmish, worldState, hotkeys, rallyPos, worldActive, worldIs2v2, sandboxMode,
            worldDifficulty, worldMapPath, lastOutcomeState, replayCursor, replayPlayTimer,
            pendingConfirm)
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
    cheats.Init();

    InitAudioDevice();
    audio.Init(IsAudioDeviceReady());
    art.Init(true);
    if (art.HasFont())
    {
        GuiSetFont(art.UiFont());
    }
    rmlUi.Init("data/ui");
    if (rmlUi.IsReady() && !rmlUiMenus.Init(rmlUi, "data/ui"))
    {
        // Menu documents failed: fall back to the pure raygui branch.
        rmlUi.Shutdown();
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

    bindings.Bind();
}

void Game::Update()
{
    if (pendingConfirm != ConfirmChoice::None)
    {
        match.ApplyConfirmChoice(pendingConfirm);
        pendingConfirm = ConfirmChoice::None;
    }
    match.PollConfirmKeys();
    input.shortcuts.SetEnabled(!menu.ConfirmOpen() && !cheats.CapturingInput());
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
        if (rmlUiMenus.HandlesState())
        {
            if (const ConfirmChoice choice = rmlUiMenus.Draw(screenWidth, screenHeight);
                choice != ConfirmChoice::None)
            {
                pendingConfirm = choice;
            }
            return;
        }
        rmlUiMenus.HideAll();
        if (const ConfirmChoice choice = menuScreens.Draw(screenWidth, screenHeight, menuStateTime);
            choice != ConfirmChoice::None)
        {
            pendingConfirm = choice;
        }
        return;
    }

    if (menu.state == MenuState::Playing && !menu.ConfirmOpen() && !cheats.CapturingInput())
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
                match.StepReplay(1);
            }
        }
    }
    pings.Update(GetFrameTime());
    shakeTrauma = DecayShakeTrauma(shakeTrauma, GetFrameTime());

    BeginDrawing();
    ClearBackground(RAYWHITE);

    renderer.DrawWorld();

    pendingConfirm = renderer.DrawHudAndOverlays(screenWidth, screenHeight, menu.settings.uiScale);
    cheats.Frame();
    EndDrawing();
}

void Game::Shutdown()
{
    minimap.Unload();
    art.Shutdown();
    audio.Shutdown();
    CloseAudioDevice();
    cheats.Shutdown();
    rmlUi.Shutdown();
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

void Game::StartMatch(const std::string &mapPath, AIDifficulty difficulty)
{
    match.StartMatch(mapPath, difficulty);
}
void Game::QuitToMenu()
{
    match.QuitToMenu();
}
