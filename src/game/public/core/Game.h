#pragma once

#include <string>

#include "raylib.h"
#include "units/AICommander.h"
#include "app/data/Art.h"
#include "core/CheatOverlay.h"
#include "core/GameRenderer.h"
#include "core/MatchController.h"
#include "app/match/Menu.h"
#include "core/MenuScreens.h"
#include "app/ui/RmlUiHost.h"
#include "app/ui/RmlUiHud.h"
#include "app/ui/RmlUiMenus.h"
#include "app/save/SaveGame.h"
#include "core/ShortcutBindings.h"
#include "core/Simulation.h"
#include "app/match/Skirmish.h"
#include "core/Subsystem.h"

// Non-copyable: shortcut lambdas capture this.
class Game
{
public:
	Game();
	Game(const Game &) = delete;
	Game &operator=(const Game &) = delete;

	void Init();
	void Update();
	void Shutdown();
	bool IsRunning() const;
	int Run();

private:
	// tests/e2e/E2EGame drives the real loop headless: it reads world_, menu
	// and worldActive directly.
	friend class E2EGame;

	// Engine-lifetime service scope: owns Audio, Art, EventDispatcher,
	// InputManager, and HotkeyMap. First member, constructed before every
	// consumer, so init-list expressions may bind engine_.Get<T>() references.
	// The Adds run in the EngineScope ctor; Game::Init body would be too late.
	struct EngineScope : public Subsystems
	{
		EngineScope();
	};
	EngineScope engine_;

	// World/match-lifetime service scope: owns Registry, ResourceSystem,
	// TileMap, OccupancyGrid, FogOfWar, ResourceNodes, two keyed ProductionQueues,
	// UnitFactory, three keyed AICommanders, Pings, and Minimap. Declared
	// after engine_ (factory/AI bind the engine EventDispatcher) and before
	// every consumer, so init-list expressions may bind world_.Get<T>().
	struct WorldScope : public Subsystems
	{
		explicit WorldScope(Subsystems &engine);
	};
	WorldScope world_{engine_};

	MenuFlow menu; // before player_: PlayerScope binds menu.settings

	// Player/view-state scope: owns GameCamera + PlayingInput. Declared after
	// engine_/world_/menu (PlayingInput binds them all) and before
	// every consumer, so init-list expressions may bind player_.Get<T>().
	struct PlayerScope : public Subsystems
	{
		PlayerScope(Subsystems &engine, Subsystems &world, const MenuSettings &settings);
	};
	PlayerScope player_{engine_, world_, menu.settings};

	SkirmishWorld skirmish;
	WorldState worldState;
	bool worldActive = false;
	bool worldIs2v2 = false;
	bool sandboxMode = false;
	AIDifficulty worldDifficulty = AIDifficulty::Medium;
	std::string worldMapPath;

	int replayCursor = 0;
	float replayPlayTimer = 0.0f;
	float shakeTrauma = 0.0f; // 0..1, render copy only
	DamageNumbers damageNumbers;
	MenuState previousMenuState = MenuState::MainMenu;
	float menuStateTime = 0.0f;
	MenuState lastOutcomeState = MenuState::MainMenu;
	ConfirmChoice pendingConfirm = ConfirmChoice::None; // resolved next frame

	// Match tick (declared after the members it binds).
	Simulation sim;
	MenuScreens menuScreens;
	// Declared before bindings: the Back shortcut prefers the RML remap
	// capture while it owns the HotkeyRemap state.
	RmlUiMenus rmlUiMenus;
	ShortcutBindings bindings;
	RmlUiHost rmlUi; // declared before renderer: overlay host (Phase 1 render-only)
	RmlUiHud rmlUiHud; // in-match HUD panels (Phase 3, raygui fallback when not ready)
	GameRenderer renderer;
	MatchController match;
	CheatOverlay cheats;

	// Match lifecycle the e2e harness drives directly (through the friend above).
	void StartMatch(const std::string &mapPath, AIDifficulty difficulty);
	void QuitToMenu();
};
