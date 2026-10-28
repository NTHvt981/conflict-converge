
#include "core/Game.h"

#include "raygui.h"
#include "app/data/Audio.h"
#include "app/data/DataRoot.h"
#include "app/data/Log.h"
#include "core/Event.h"
#include "world/FogOfWar.h"
#include "app/ui/GameCamera.h"
#include "app/input/Hotkeys.h"
#include "app/input/InputManager.h"
#include "core/MathUtils.h"
#include "app/ui/Minimap.h"
#include "economy/Nodes.h"
#include "app/ui/Pings.h"
#include "core/PlayingInput.h"
#include "economy/Production.h"
#include "core/Registry.h"
#include "economy/ResourceSystem.h"
#include "app/ui/Shake.h"
#include "world/TileMap.h"
#include "units/UnitFactory.h"

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
	AddKeyed<ProductionQueue>("bootcamp", ProductionCategory::Infantry);
	AddKeyed<ProductionQueue>("workshop", ProductionCategory::Vehicle);
	EventDispatcher &events = engine.Get<EventDispatcher>();
	Add<UnitFactory>(Get<Registry>(), Get<ResourceSystem>(), events);
	AddKeyed<AICommander>("ai", Get<Registry>(), Get<TileMap>(), Get<ResourceNodes>(), events,
						  kEnemyTeam, AIDifficulty::Medium, cc::IVec2{ 0, 0 }, cc::IVec2{ 0, 0 });
	AddKeyed<AICommander>("ally", Get<Registry>(), Get<TileMap>(), Get<ResourceNodes>(), events,
						  kPlayerTeam, AIDifficulty::Medium, cc::IVec2{ 0, 0 }, cc::IVec2{ 0, 0 });
	AddKeyed<AICommander>("enemy2", Get<Registry>(), Get<TileMap>(), Get<ResourceNodes>(),
						  events, kEnemyTeam, AIDifficulty::Medium, cc::IVec2{ 0, 0 },
						  cc::IVec2{ 0, 0 });
	Add<Pings>();
	Add<Minimap>();
}

Game::PlayerScope::PlayerScope(Subsystems &engine, Subsystems &world,
							   const MenuSettings &settings)
{
	Add<GameCamera>();
	Add<PlayingInput>(world.Get<Registry>(), world.Get<TileMap>(), world.Get<OccupancyGrid>(),
					  world.Get<ResourceNodes>(), Get<GameCamera>(), world.Get<Minimap>(),
					  engine.Get<InputManager>(), engine.Get<Audio>(), settings,
					  world.Get<ResourceSystem>());
}

Game::Game()
	: skirmish_{ &world_.Get<Registry>(), &world_.Get<ResourceSystem>(),
				&world_.Get<TileMap>(), &world_.Get<OccupancyGrid>(),
				&world_.Get<FogOfWar>(), &world_.Get<ResourceNodes>(),
				&world_.GetKeyed<ProductionQueue>("bootcamp"),
				&world_.GetKeyed<ProductionQueue>("workshop"), &world_.Get<UnitFactory>(),
				&world_.GetKeyed<AICommander>("ai"), &world_.GetKeyed<AICommander>("ally"),
				&world_.GetKeyed<AICommander>("enemy2"), &player_.Get<GameCamera>() }
	, worldState_{ &world_.Get<Registry>(), &world_.Get<ResourceSystem>(),
				  &world_.Get<TileMap>(), &player_.Get<GameCamera>(),
				  &world_.Get<ResourceNodes>(), &world_.Get<FogOfWar>(),
				  &world_.Get<OccupancyGrid>() }
	, sim_(world_, engine_, menu_, worldState_, damageNumbers_,
		  player_.Get<PlayingInput>(), sandboxMode_, worldIs2v2_,
		  shakeTrauma_, lastOutcomeState_)
	, menuScreens_(menu_, engine_.Get<Art>(), engine_.Get<Audio>(), engine_.Get<InputManager>(),
				  engine_.Get<HotkeyMap>(), engine_.Get<EventDispatcher>(),
				  MenuCallbacks{
					  [&](const std::string &mapPath, AIDifficulty difficulty) {
						  StartMatch(mapPath, difficulty);
					  },
					  [&](const std::string &slotPath) { match_.LoadGameFromSlot(slotPath); },
					  [&]() { match_.WatchLastReplay(); },
					  [&]() { bindings_.Bind(); },
				  })
	, rmlUiMenus_(menu_, engine_.Get<HotkeyMap>(), engine_.Get<Art>(), engine_.Get<Audio>(),
				 engine_.Get<EventDispatcher>(), menuScreens_,
				 MenuCallbacks{
					 [&](const std::string &mapPath, AIDifficulty difficulty) {
						 StartMatch(mapPath, difficulty);
					 },
					 [&](const std::string &slotPath) { match_.LoadGameFromSlot(slotPath); },
					 [&]() { match_.WatchLastReplay(); },
					 [&]() { bindings_.Bind(); },
				 })
	, bindings_(engine_.Get<InputManager>(), engine_.Get<HotkeyMap>(), menu_, rmlUiMenus_,
				player_.Get<PlayingInput>(), engine_.Get<Audio>(),
				player_.Get<GameCamera>(), world_.Get<TileMap>(),
				world_.Get<OccupancyGrid>(), world_.Get<ResourceNodes>(), world_.Get<FogOfWar>(),
				world_.Get<Registry>(), world_.Get<ResourceSystem>(),
				 engine_.Get<EventDispatcher>(), worldState_, world_.Get<Pings>(), worldActive_,
				 menu_.settings.showHints, [this]() { QuitToMenu(); },
				 [this](int dir) { match_.StepReplay(dir); })
	, rmlUiHud_(world_.Get<Registry>(), world_.Get<ResourceSystem>(),
			   world_.GetKeyed<ProductionQueue>("bootcamp"),
			   world_.GetKeyed<ProductionQueue>("workshop"), sim_, engine_.Get<HotkeyMap>(),
			   player_.Get<PlayingInput>(), world_.Get<TileMap>(),
			   world_.Get<OccupancyGrid>(), menu_.settings.showHints,
			   menu_, engine_.Get<Art>(),
			   engine_.Get<EventDispatcher>(),
			   [this]() { QuitToMenu(); },
			   [this](MenuState returnTo) { rmlUiMenus_.BeginRemap(returnTo); })
	 , renderer_(engine_.Get<Art>(), player_.Get<GameCamera>(), world_.Get<TileMap>(), world_.Get<Registry>(),
			   world_.Get<FogOfWar>(), world_.Get<ResourceNodes>(), world_.Get<Minimap>(),
			   world_.Get<Pings>(), damageNumbers_, player_.Get<PlayingInput>(), menu_, sim_,
			   world_.Get<ResourceSystem>(),
			   engine_.Get<HotkeyMap>(), engine_.Get<InputManager>(),
			   world_.GetKeyed<AICommander>("ai"),
			   engine_.Get<EventDispatcher>(), replayCursor_, worldDifficulty_, menuStateTime_,
			   shakeTrauma_, rmlUi_, rmlUiHud_, rmlUiMenus_,
			   [this]() { QuitToMenu(); })
	, match_(world_, player_.Get<GameCamera>(), world_.GetKeyed<AICommander>("ai"),
			world_.GetKeyed<AICommander>("ally"),
			world_.GetKeyed<AICommander>("enemy2"), menu_, world_.Get<Minimap>(),
			player_.Get<PlayingInput>(), sim_,
			engine_.Get<EventDispatcher>(), skirmish_, worldState_, engine_.Get<HotkeyMap>(),
			worldActive_, worldIs2v2_, sandboxMode_, worldDifficulty_, worldMapPath_,
			lastOutcomeState_, replayCursor_, replayPlayTimer_, pendingConfirm_)
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
	cheats_.Init();

	InitAudioDevice();
	engine_.Get<Audio>().Init(IsAudioDeviceReady() ? InitMode::WithDevice : InitMode::Headless);
	engine_.Get<Art>().Init(InitMode::WithDevice);
	if (engine_.Get<Art>().HasFont())
	{
		GuiSetFont(engine_.Get<Art>().UiFont());
	}
	rmlUi_.Init("data/ui", "data/fonts");
	if (!rmlUiMenus_.Init(rmlUi_, "data/ui"))
	{
		Log::Fatal("RmlUi menu documents failed to load from data/ui");
	}
	if (!rmlUiHud_.Init(rmlUi_, "data/ui"))
	{
		Log::Fatal("RmlUi HUD documents failed to load from data/ui");
	}
	lastOutcomeState_ = MenuState::MainMenu;

	player_.Get<GameCamera>().view.offset = { kInitialWidth / 2.0f, kInitialHeight / 2.0f };
	player_.Get<GameCamera>().view.rotation = 0.0f;
	player_.Get<GameCamera>().view.zoom = 1.0f;
	LoadSettings(menu_.settings, kSettingsPath);
	engine_.Get<Art>().SetColorBlindMode(menu_.settings.colorBlindMode);
	GuiSetStyle(DEFAULT, TEXT_SIZE, static_cast<int>(10 * menu_.settings.uiScale));
	menuScreens_.ApplyHotkeyOverrides();

	world_.Get<Minimap>().Init(
		Minimap::TopRightSquare(kInitialWidth, kInitialHeight));

	worldActive_ = false;
	worldIs2v2_ = false;
	worldDifficulty_ = AIDifficulty::Medium;
	worldMapPath_.clear();

	engine_.Get<EventDispatcher>().Subscribe(EventType::UnitSpawned, [&](const Event &) {
		engine_.Get<Audio>().Play(SfxId::Confirm);
	});
	engine_.Get<EventDispatcher>().Subscribe(EventType::UnitDestroyed, [&](const Event &e) {
		const auto *lifecycle = dynamic_cast<const UnitLifecycleEvent *>(&e);
		if (lifecycle != nullptr && lifecycle->teamID == kPlayerTeam)
		{
			world_.Get<Pings>().Raise(
				{ lifecycle->position.x + cc::kHalfTile, lifecycle->position.y + cc::kHalfTile },
				PingKind::UnitLost);
		}
	});

	bindings_.Bind();
}

void Game::Update()
{
	if (pendingConfirm_ != ConfirmChoice::None)
	{
		match_.ApplyConfirmChoice(pendingConfirm_);
		pendingConfirm_ = ConfirmChoice::None;
	}
	match_.PollConfirmKeys();
#if defined(CC_DEBUG)
	// Hard-wired UI debug keys: deliberately outside the remappable hotkeys.
	if (IsKeyPressed(KEY_F10))
	{
		rmlUi_.ToggleDebugger();
	}
	if (IsKeyPressed(KEY_F11))
	{
		rmlUiHud_.ReloadStyleSheets();
		rmlUiMenus_.ReloadStyleSheets();
	}
#endif
	engine_.Get<InputManager>().shortcuts.SetEnabled(!menu_.ConfirmOpen() && !cheats_.CapturingInput());
	engine_.Get<InputManager>().Update(player_.Get<GameCamera>(), menu_.settings.cameraSpeed,
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
	TrackMenuTransition(previousMenuState_, menuStateTime_, menu_.state, GetFrameTime());

	if (!worldActive_)
	{
		rmlUiHud_.Hide();
		if (rmlUiMenus_.HandlesState())
		{
			if (const ConfirmChoice choice = rmlUiMenus_.Draw(screenWidth, screenHeight);
				choice != ConfirmChoice::None)
			{
				pendingConfirm_ = choice;
			}
			return;
		}
		rmlUiMenus_.HideAll();
		if (const ConfirmChoice choice = menuScreens_.Draw(screenWidth, screenHeight, menuStateTime_);
			choice != ConfirmChoice::None)
		{
			pendingConfirm_ = choice;
		}
		return;
	}

	// Hide menu-branch documents: entering a match otherwise leaves them
	// shown over the game.
	rmlUiMenus_.HideAll();

	double simMs = 0.0;
	if (menu_.state == MenuState::Playing && !menu_.ConfirmOpen() && !cheats_.CapturingInput())
	{
		player_.Get<PlayingInput>().Dispatch(rmlUiHud_.IsPointerOverUI());
		const double simStart = GetTime();
		sim_.Step(GetFrameTime());
		simMs = (GetTime() - simStart) * 1000.0;
	}

	engine_.Get<Audio>().ApplySettings(menu_.settings.masterVolume, menu_.settings.musicVolume,
								  menu_.settings.sfxVolume, menu_.settings.mute);
	engine_.Get<Audio>().UpdateMusic();

	if (menu_.state == MenuState::ReplayViewer && sim_.ReplayCount() > 0)
	{
		replayPlayTimer_ += GetFrameTime();
		if (replayPlayTimer_ >= 2.0f)
		{
			replayPlayTimer_ = 0.0f;
			if (replayCursor_ + 1 < sim_.ReplayCount())
			{
				match_.StepReplay(1);
			}
		}
	}
	world_.Get<Pings>().Update(GetFrameTime());
	shakeTrauma_ = DecayShakeTrauma(shakeTrauma_, GetFrameTime());

	BeginDrawing();
	ClearBackground(RAYWHITE);

	renderer_.DrawWorld();

	pendingConfirm_ = renderer_.DrawHudAndOverlays(screenWidth, screenHeight, menu_.settings.uiScale);
	cheats_.Frame(PerfSample{ simMs, rmlUi_.LastUpdateMs(), rmlUi_.LastRenderMs(), rmlUi_.LastTriangles() });
	EndDrawing();
}

void Game::Shutdown()
{
	world_.Get<Minimap>().Unload();
	engine_.Get<Art>().Shutdown();
	engine_.Get<Audio>().Shutdown();
	CloseAudioDevice();
	cheats_.Shutdown();
	rmlUi_.Shutdown();
	CloseWindow();
	Log::Shutdown();
	world_.Shutdown();
	engine_.Shutdown();
}

bool Game::IsRunning() const
{
	return !WindowShouldClose() && !menu_.quitRequested;
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
	match_.StartMatch(mapPath, difficulty);
}
void Game::QuitToMenu()
{
	match_.QuitToMenu();
}
