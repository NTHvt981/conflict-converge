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
	// tests/e2e/E2EGame drives the real loop headless: it reads world_, menu_
	// and worldActive_ directly.
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

	MenuFlow menu_; // before player_: PlayerScope binds menu_.settings

	// Player/view-state scope: owns GameCamera + PlayingInput. Declared after
	// engine_/world_/menu_ (PlayingInput binds them all) and before
	// every consumer, so init-list expressions may bind player_.Get<T>().
	struct PlayerScope : public Subsystems
	{
		PlayerScope(Subsystems &engine, Subsystems &world, const MenuSettings &settings);
	};
	PlayerScope player_{engine_, world_, menu_.settings};

	SkirmishWorld skirmish_;
	WorldState worldState_;
	bool worldActive_ = false;
	bool worldIs2v2_ = false;
	bool sandboxMode_ = false;
	AIDifficulty worldDifficulty_ = AIDifficulty::Medium;
	std::string worldMapPath_;

	int replayCursor_ = 0;
	float replayPlayTimer_ = 0.0f;
	float shakeTrauma_ = 0.0f; // 0..1, render copy only
	DamageNumbers damageNumbers_;
	MenuState previousMenuState_ = MenuState::MainMenu;
	float menuStateTime_ = 0.0f;
	MenuState lastOutcomeState_ = MenuState::MainMenu;
	ConfirmChoice pendingConfirm_ = ConfirmChoice::None; // resolved next frame

	// Match tick (declared after the members it binds).
	Simulation sim_;
	MenuScreens menuScreens_;
	// Declared before bindings_: the Back shortcut prefers the RML remap
	// capture while it owns the HotkeyRemap state.
	RmlUiMenus rmlUiMenus_;
	ShortcutBindings bindings_;
	RmlUiHost rmlUi_; // declared before renderer_: overlay host (Phase 1 render-only)
	RmlUiHud rmlUiHud_; // in-match HUD panels (Phase 3, raygui fallback when not ready)
	GameRenderer renderer_;
	MatchController match_;
	CheatOverlay cheats_;

	// Match lifecycle the e2e harness drives directly (through the friend above).
	void StartMatch(const std::string &mapPath, AIDifficulty difficulty);
	void QuitToMenu();
};
