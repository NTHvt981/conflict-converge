
#include "Game.h"

#include "raygui.h"
#include "DataRoot.h"
#include "Log.h"

Game::EngineScope::EngineScope()
{
    Add<Audio>();
    Add<Art>();
    Add<EventDispatcher>();
    Add<InputManager>();
    Add<HotkeyMap>();
}

Game::Game()
    : map(20, 15)
    , occ(20, 15)
    , factory(registry, resources, engine_.Get<EventDispatcher>())
    , ai(registry, map, nodes, engine_.Get<EventDispatcher>(), 1, AIDifficulty::Medium, { 0, 0 }, { 0, 0 })
    , allyAI(registry, map, nodes, engine_.Get<EventDispatcher>(), 0, AIDifficulty::Medium, { 0, 0 }, { 0, 0 })
    , enemyAI2(registry, map, nodes, engine_.Get<EventDispatcher>(), 1, AIDifficulty::Medium, { 0, 0 }, { 0, 0 })
    , skirmish{ &registry, &resources, &map, &occ, &fog, &nodes,
                &queue,   &factory,   &ai, &allyAI, &enemyAI2, &camera, &rallyPos }
    , worldState{ &registry, &resources, &map, &camera, &nodes, &fog, &occ }
    , playingInput(registry, map, occ, nodes, camera, minimap, engine_.Get<InputManager>(),
                   engine_.Get<Audio>(), menu.settings, rallyPos)
    , sim(registry, map, occ, fog, nodes, queue, factory, resources, ai, allyAI, enemyAI2,
          engine_.Get<Art>(), engine_.Get<Audio>(), pings, menu, minimap, worldState,
          damageNumbers, engine_.Get<EventDispatcher>(), rallyPos,
          playingInput.AutoAddGroupBit(), sandboxMode, worldIs2v2, playerAutoRepair,
          autoRepairCap, shakeTrauma, lastOutcomeState)
    , menuScreens(menu, engine_.Get<Art>(), engine_.Get<Audio>(), engine_.Get<InputManager>(),
                  engine_.Get<HotkeyMap>(), engine_.Get<EventDispatcher>(),
                  MenuCallbacks{
                      [&](const std::string &mapPath, AIDifficulty difficulty) {
                          StartMatch(mapPath, difficulty);
                      },
                      [&](const std::string &slotPath) { match.LoadGameFromSlot(slotPath); },
                      [&]() { match.WatchLastReplay(); },
                      [&]() { bindings.Bind(); },
                  })
    , rmlUiMenus(menu, engine_.Get<HotkeyMap>(), engine_.Get<Art>(), engine_.Get<Audio>(),
                 engine_.Get<EventDispatcher>(), menuScreens,
                 MenuCallbacks{
                     [&](const std::string &mapPath, AIDifficulty difficulty) {
                         StartMatch(mapPath, difficulty);
                     },
                     [&](const std::string &slotPath) { match.LoadGameFromSlot(slotPath); },
                     [&]() { match.WatchLastReplay(); },
                     [&]() { bindings.Bind(); },
                 })
    , bindings(engine_.Get<InputManager>(), engine_.Get<HotkeyMap>(), menu, rmlUiMenus,
                playingInput, engine_.Get<Audio>(), camera, map, occ, nodes, fog, registry,
                resources, engine_.Get<EventDispatcher>(), worldState, pings, worldActive,
                showHints, [this]() { QuitToMenu(); },
                [this](int dir) { match.StepReplay(dir); })
    , rmlUiHud(registry, resources, queue, sim, engine_.Get<HotkeyMap>(), playingInput, ai,
               worldDifficulty, showHints, playerAutoRepair, autoRepairCap, menu,
               engine_.Get<Art>(), engine_.Get<EventDispatcher>(),
               [this]() { QuitToMenu(); },
               [this](MenuState returnTo) { rmlUiMenus.BeginRemap(returnTo); })
    , renderer(engine_.Get<Art>(), camera, map, registry, fog, nodes, minimap, pings,
               damageNumbers, playingInput, menu, sim, resources, queue,
               engine_.Get<HotkeyMap>(), engine_.Get<InputManager>(), ai,
               engine_.Get<EventDispatcher>(), replayCursor, worldDifficulty, menuStateTime,
               shakeTrauma, rmlUi, rmlUiHud, rmlUiMenus,
               [this]() { QuitToMenu(); })
    , match(camera, ai, allyAI, enemyAI2, menu, minimap, playingInput, sim,
            engine_.Get<EventDispatcher>(), skirmish, worldState, engine_.Get<HotkeyMap>(),
            rallyPos, worldActive, worldIs2v2, sandboxMode, worldDifficulty, worldMapPath,
            lastOutcomeState, replayCursor, replayPlayTimer, pendingConfirm)
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
    engine_.Get<Audio>().Init(IsAudioDeviceReady());
    engine_.Get<Art>().Init(true);
    if (engine_.Get<Art>().HasFont())
    {
        GuiSetFont(engine_.Get<Art>().UiFont());
    }
    rmlUi.Init("data/ui", "data/fonts");
    if (!rmlUiMenus.Init(rmlUi, "data/ui"))
    {
        Log::Fatal("RmlUi menu documents failed to load from data/ui");
    }
    if (!rmlUiHud.Init(rmlUi, "data/ui"))
    {
        Log::Fatal("RmlUi HUD documents failed to load from data/ui");
    }
    lastOutcomeState = MenuState::MainMenu;

    camera.view.offset = { kInitialWidth / 2.0f, kInitialHeight / 2.0f };
    camera.view.rotation = 0.0f;
    camera.view.zoom = 1.0f;
    LoadSettings(menu.settings, kSettingsPath);
    engine_.Get<Art>().SetColorBlindMode(menu.settings.colorBlindMode);
    GuiSetStyle(DEFAULT, TEXT_SIZE, static_cast<int>(10 * menu.settings.uiScale));
    menuScreens.ApplyHotkeyOverrides();

    minimap.Init({ static_cast<float>(kInitialWidth) - 170.0f, 10.0f,
                   160.0f, 120.0f });

    worldActive = false;
    worldIs2v2 = false;
    worldDifficulty = AIDifficulty::Medium;
    worldMapPath.clear();
    showHints = true;

    engine_.Get<EventDispatcher>().Subscribe(EventType::UnitSpawned, [&](const Event &) {
        engine_.Get<Audio>().Play(SfxId::Confirm);
    });
    engine_.Get<EventDispatcher>().Subscribe(EventType::UnitDestroyed, [&](const Event &e) {
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
    engine_.Get<InputManager>().shortcuts.SetEnabled(!menu.ConfirmOpen() && !cheats.CapturingInput());
    engine_.Get<InputManager>().Update(camera, menu.settings.cameraSpeed, GetFrameTime());
    camera.AdjustZoom(engine_.Get<InputManager>().WheelDelta());
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
        rmlUiHud.Hide();
        if (rmlUiMenus.HandlesState())
        {
            if (const ConfirmChoice choice = rmlUiMenus.Draw(screenWidth, screenHeight);
                choice != ConfirmChoice::None)
            {
                pendingConfirm = choice;
            }
            return;
        }
        // Anything RmlUiMenus does not handle is the raygui Map Editor.
        rmlUiMenus.HideAll();
        if (const ConfirmChoice choice = menuScreens.Draw(screenWidth, screenHeight, menuStateTime);
            choice != ConfirmChoice::None)
        {
            pendingConfirm = choice;
        }
        return;
    }

    // Menu documents never draw in the world branch: hide any left visible
    // by the menu branch (entering a match otherwise leaves them shown over
    // the game). Idempotent while raygui owns the menus.
    rmlUiMenus.HideAll();

    if (menu.state == MenuState::Playing && !menu.ConfirmOpen() && !cheats.CapturingInput())
    {
        // First-refusal preview (Phase 4 owns full routing): presses on RmlUi
        // controls never reach world dispatch. The sim always steps.
        if (!rmlUiHud.IsPointerOverUI())
        {
            playingInput.Dispatch();
        }
        sim.Step(GetFrameTime());
    }

    engine_.Get<Audio>().ApplySettings(menu.settings.masterVolume, menu.settings.musicVolume,
                                  menu.settings.sfxVolume, menu.settings.mute);
    engine_.Get<Audio>().UpdateMusic();

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
    engine_.Get<Art>().Shutdown();
    engine_.Get<Audio>().Shutdown();
    CloseAudioDevice();
    cheats.Shutdown();
    rmlUi.Shutdown();
    CloseWindow();
    Log::Shutdown();
    engine_.Shutdown();
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
