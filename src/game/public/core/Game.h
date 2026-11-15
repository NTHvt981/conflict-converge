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
	friend class E2EGame;

	struct EngineScope : public Subsystems
	{
		EngineScope();
	};
	EngineScope engine_;

	struct WorldScope : public Subsystems
	{
		explicit WorldScope(Subsystems &engine);
	};
	WorldScope world_{engine_};

	MenuFlow menu_;

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
	MenuState previousMenuState_ = MenuState::MainMenu;
	float menuStateTime_ = 0.0f;
	MenuState lastOutcomeState_ = MenuState::MainMenu;
	ConfirmChoice pendingConfirm_ = ConfirmChoice::None; // resolved next frame

	Simulation sim_;
	MenuScreens menuScreens_;
	RmlUiMenus rmlUiMenus_;
	ShortcutBindings bindings_;
	RmlUiHost rmlUi_;
	RmlUiHud rmlUiHud_;
	GameRenderer renderer_;
	MatchController match_;
	CheatOverlay cheats_;

	void StartMatch(const std::string &mapPath, AIDifficulty difficulty);
	void QuitToMenu();
};
