
#include "core/Game.h"

#include "raygui.h"
#include "app/DataRoot.h"
#include "app/Log.h"

Game::EngineScope::EngineScope()
{
    Add<Audio>();
    Add<Art>();
    Add<EventDispatcher>();
    Add<InputManager>();
    Add<HotkeyMap>();
}

Game::WorldScope::WorldScope(Subsystems &engine)
{
    Add<Registry>();
    Add<ResourceSystem>();
    Add<TileMap>(20, 15);
    Add<OccupancyGrid>(20, 15);
    Add<FogOfWar>();
    Add<ResourceNodes>();
    Add<ProductionQueue>();
    EventDispatcher &events = engine.Get<EventDispatcher>();
    Add<UnitFactory>(Get<Registry>(), Get<ResourceSystem>(), events);
    AddKeyed<AICommander>("ai", Get<Registry>(), Get<TileMap>(), Get<ResourceNodes>(), events,
                          1, AIDifficulty::Medium, cc::IVec2{ 0, 0 }, cc::IVec2{ 0, 0 });
    AddKeyed<AICommander>("ally", Get<Registry>(), Get<TileMap>(), Get<ResourceNodes>(), events,
                          0, AIDifficulty::Medium, cc::IVec2{ 0, 0 }, cc::IVec2{ 0, 0 });
    AddKeyed<AICommander>("enemy2", Get<Registry>(), Get<TileMap>(), Get<ResourceNodes>(),
                          events, 1, AIDifficulty::Medium, cc::IVec2{ 0, 0 }, cc::IVec2{ 0, 0 });
    Add<Pings>();
    Add<Minimap>();
}

Game::PlayerScope::PlayerScope(Subsystems &engine, Subsystems &world,
                               const MenuSettings &settings, Vector2 &rallyPos)
{
    Add<GameCamera>();
    Add<PlayingInput>(world.Get<Registry>(), world.Get<TileMap>(), world.Get<OccupancyGrid>(),
                      world.Get<ResourceNodes>(), Get<GameCamera>(), world.Get<Minimap>(),
                      engine.Get<InputManager>(), engine.Get<Audio>(), settings, rallyPos);
}

Game::Game()
    : skirmish{ &world_.Get<Registry>(), &world_.Get<ResourceSystem>(),
                &world_.Get<TileMap>(), &world_.Get<OccupancyGrid>(),
                &world_.Get<FogOfWar>(), &world_.Get<ResourceNodes>(),
                &world_.Get<ProductionQueue>(), &world_.Get<UnitFactory>(),
                &world_.GetKeyed<AICommander>("ai"), &world_.GetKeyed<AICommander>("ally"),
                &world_.GetKeyed<AICommander>("enemy2"), &player_.Get<GameCamera>(),
                &rallyPos }
    , worldState{ &world_.Get<Registry>(), &world_.Get<ResourceSystem>(),
                  &world_.Get<TileMap>(), &player_.Get<GameCamera>(),
                  &world_.Get<ResourceNodes>(), &world_.Get<FogOfWar>(),
                  &world_.Get<OccupancyGrid>() }
    , sim(world_, engine_, menu, worldState, damageNumbers, rallyPos,
          player_.Get<PlayingInput>().AutoAddGroupBit(), sandboxMode, worldIs2v2,
          shakeTrauma, lastOutcomeState)
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
                player_.Get<PlayingInput>(), engine_.Get<Audio>(),
                player_.Get<GameCamera>(), world_.Get<TileMap>(),
                world_.Get<OccupancyGrid>(), world_.Get<ResourceNodes>(), world_.Get<FogOfWar>(),
                world_.Get<Registry>(), world_.Get<ResourceSystem>(),
                 engine_.Get<EventDispatcher>(), worldState, world_.Get<Pings>(), worldActive,
                 menu.settings.showHints, [this]() { QuitToMenu(); },
                 [this](int dir) { match.StepReplay(dir); })
    , rmlUiHud(world_.Get<Registry>(), world_.Get<ResourceSystem>(),
               world_.Get<ProductionQueue>(), sim, engine_.Get<HotkeyMap>(),
               player_.Get<PlayingInput>(), world_.Get<TileMap>(),
               world_.Get<OccupancyGrid>(), menu.settings.showHints,
               menu, engine_.Get<Art>(),
               engine_.Get<EventDispatcher>(),
               [this]() { QuitToMenu(); },
               [this](MenuState returnTo) { rmlUiMenus.BeginRemap(returnTo); })
    , renderer(engine_.Get<Art>(), player_.Get<GameCamera>(), world_.Get<TileMap>(), world_.Get<Registry>(),
               world_.Get<FogOfWar>(), world_.Get<ResourceNodes>(), world_.Get<Minimap>(),
               world_.Get<Pings>(), damageNumbers, player_.Get<PlayingInput>(), menu, sim,
               world_.Get<ResourceSystem>(), world_.Get<ProductionQueue>(),
               engine_.Get<HotkeyMap>(), engine_.Get<InputManager>(),
               world_.GetKeyed<AICommander>("ai"),
               engine_.Get<EventDispatcher>(), replayCursor, worldDifficulty, menuStateTime,
               shakeTrauma, rmlUi, rmlUiHud, rmlUiMenus,
               [this]() { QuitToMenu(); })
    , match(world_, player_.Get<GameCamera>(), world_.GetKeyed<AICommander>("ai"),
            world_.GetKeyed<AICommander>("ally"),
            world_.GetKeyed<AICommander>("enemy2"), menu, world_.Get<Minimap>(),
            player_.Get<PlayingInput>(), sim,
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

    player_.Get<GameCamera>().view.offset = { kInitialWidth / 2.0f, kInitialHeight / 2.0f };
    player_.Get<GameCamera>().view.rotation = 0.0f;
    player_.Get<GameCamera>().view.zoom = 1.0f;
    LoadSettings(menu.settings, kSettingsPath);
    engine_.Get<Art>().SetColorBlindMode(menu.settings.colorBlindMode);
    GuiSetStyle(DEFAULT, TEXT_SIZE, static_cast<int>(10 * menu.settings.uiScale));
    menuScreens.ApplyHotkeyOverrides();

    world_.Get<Minimap>().Init(
        Minimap::TopRightSquare(kInitialWidth, kInitialHeight));

    worldActive = false;
    worldIs2v2 = false;
    worldDifficulty = AIDifficulty::Medium;
    worldMapPath.clear();

    engine_.Get<EventDispatcher>().Subscribe(EventType::UnitSpawned, [&](const Event &) {
        engine_.Get<Audio>().Play(SfxId::Confirm);
    });
    engine_.Get<EventDispatcher>().Subscribe(EventType::UnitDestroyed, [&](const Event &e) {
        const auto *lifecycle = dynamic_cast<const UnitLifecycleEvent *>(&e);
        if (lifecycle != nullptr && lifecycle->teamID == 0)
        {
            world_.Get<Pings>().Raise({ lifecycle->position.x + 32.0f, lifecycle->position.y + 32.0f },
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
    engine_.Get<InputManager>().Update(player_.Get<GameCamera>(), menu.settings.cameraSpeed,
                                       GetFrameTime());
    player_.Get<GameCamera>().AdjustZoom(engine_.Get<InputManager>().WheelDelta());
    const int screenWidth = GetScreenWidth();
    const int screenHeight = GetScreenHeight();
    player_.Get<GameCamera>().view.offset = { screenWidth / 2.0f, screenHeight / 2.0f };
    Minimap &minimap = world_.Get<Minimap>();
    const Rectangle wantMinimap = Minimap::TopRightSquare(screenWidth, screenHeight);
    if (wantMinimap.width != minimap.screenRect.width ||
        wantMinimap.height != minimap.screenRect.height)
    {
        if (wantMinimap.width >= 1.0f && wantMinimap.height >= 1.0f)
        {
            minimap.Init(wantMinimap);
        }
    }
    else
    {
        minimap.screenRect.x = wantMinimap.x;
        minimap.screenRect.y = wantMinimap.y;
    }
    player_.Get<GameCamera>().ClampZoomToWorld(static_cast<float>(world_.Get<TileMap>().Width()) * cc::TILE_SIZE,
                            static_cast<float>(world_.Get<TileMap>().Height()) * cc::TILE_SIZE,
                            screenWidth, screenHeight);
    player_.Get<GameCamera>().ClampToMap(static_cast<float>(world_.Get<TileMap>().Width()) * cc::TILE_SIZE,
                      static_cast<float>(world_.Get<TileMap>().Height()) * cc::TILE_SIZE,
                      screenWidth, screenHeight);
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
        rmlUiMenus.HideAll();
        if (const ConfirmChoice choice = menuScreens.Draw(screenWidth, screenHeight, menuStateTime);
            choice != ConfirmChoice::None)
        {
            pendingConfirm = choice;
        }
        return;
    }

    // Hide menu-branch documents: entering a match otherwise leaves them
    // shown over the game.
    rmlUiMenus.HideAll();

    if (menu.state == MenuState::Playing && !menu.ConfirmOpen() && !cheats.CapturingInput())
    {
        // Presses on RmlUi controls never reach world dispatch. The sim always steps.
        if (!rmlUiHud.IsPointerOverUI())
        {
            player_.Get<PlayingInput>().Dispatch();
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
    world_.Get<Pings>().Update(GetFrameTime());
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
    world_.Get<Minimap>().Unload();
    engine_.Get<Art>().Shutdown();
    engine_.Get<Audio>().Shutdown();
    CloseAudioDevice();
    cheats.Shutdown();
    rmlUi.Shutdown();
    CloseWindow();
    Log::Shutdown();
    world_.Shutdown();
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
