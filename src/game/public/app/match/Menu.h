#pragma once

#include <string>
#include <vector>

#include "units/AICommander.h"
#include "world/MapFile.h"
#include "core/Registry.h"
#include "core/Team.h"
#include "units/Unit.h"
#include "reasings.h"

// The sim is skipped while paused; the world builds only after Start or a slot load.

enum class MenuState
{
	MainMenu,
	SkirmishSetup,
	Settings,      // shares MenuSettings with pause
	HotkeyRemap,   // entered from Settings or pause
	LoadGame,
	Playing,
	Paused,
	GameOver,
	Victory,
	ReplayViewer, // snapshot replay: sim stays frozen
	MapEditor     // scratch-canvas painter, never the live match map
};

enum class ConfirmKind
{
	None,
	QuitApp,
	BackToMenu
};

// What the host must do after an Esc/Back press.
enum class BackAction
{
	None,                 // consumed (e.g. the modal was cancelled)
	Navigate,             // MenuFlow already changed menu state
	QuitToMenu,           // host tears down the world, then main menu
	OpenQuitConfirm,
	OpenBackToMenuConfirm
};

struct MenuSettings
{
	float cameraSpeed = 400.0f;
	bool showMinimap = true;
	bool showHints = false;
	bool rightDragPan = false;
	bool colorBlindMode = false; // Okabe-Ito team palette
	float uiScale = 1.0f;      // clamped [0.75, 2.0]
	std::vector<std::pair<std::string, int>> hotkeyOverrides;
	float masterVolume = 1.0f;
	float musicVolume = 0.8f;
	float sfxVolume = 1.0f;
	bool mute = false;
};

// Map list is injected; nothing here touches the filesystem, so it stays headless-testable.
struct SkirmishSetup
{
	std::vector<MapEntry> maps;
	int mapIndex = -1; // -1: nothing selected, Start is disabled
	AIDifficulty difficulty = AIDifficulty::Medium;

	bool CanStart() const
	{
		return mapIndex >= 0 && mapIndex < static_cast<int>(maps.size());
	}
	const MapEntry *SelectedMap() const
	{
		return CanStart() ? &maps[static_cast<std::size_t>(mapIndex)] : nullptr;
	}
};

struct MenuFlow
{
	MenuState state = MenuState::MainMenu;
	MenuSettings settings;
	SkirmishSetup setup;
	bool quitRequested = false;
	ConfirmKind confirm = ConfirmKind::None;

	bool ConfirmOpen() const { return confirm != ConfirmKind::None; }
	void OpenQuitConfirm() { confirm = ConfirmKind::QuitApp; }
	void OpenBackToMenuConfirm() { confirm = ConfirmKind::BackToMenu; }
	void CloseConfirm() { confirm = ConfirmKind::None; }

	// Esc/Back policy: opens a modal, steps a submenu back, or asks the host to tear down.
	BackAction OnBackPressed();

	// Playing <-> Paused only; outcome screens are terminal until quit.
	void TogglePause();
	// Decide terminal states from team aliveness (call each frame while Playing).
	void ShowOutcome(bool playerAlive, bool enemyAlive);
	void OpenMainMenu(); // any state -> MainMenu; world teardown is the caller's job
	void OpenSetup(const std::vector<MapEntry> &maps);
	void OpenSettings();
	void OpenLoad();
	void SelectMap(int index);
	void SelectDifficulty(AIDifficulty difficulty);
	// SkirmishSetup -> Playing when CanStart; otherwise false (stays put).
	bool StartMatch();
};

// Standalone settings file; settings survive without a save.
inline const char *kSettingsPath = "data/settings.cfg";
// Load leaves settings untouched on missing/unreadable files; malformed
// lines are skipped and every value is clamped into range.
bool SaveSettings(const MenuSettings &settings, const std::string &path);
bool LoadSettings(MenuSettings &settings, const std::string &path);

// Resets to 0 on state change, else accumulates dt; Game draws the fade.
constexpr float kMenuFadeInDuration = 0.2f;
inline void TrackMenuTransition(MenuState &previous, float &time, MenuState current, float dt)
{
	if (current != previous)
	{
		previous = current;
		time = 0.0f;
	}
	else
	{
		time += dt;
	}
}
inline float MenuFadeAlpha(float time)
{
	if (time >= kMenuFadeInDuration)
	{
		return 1.0f;
	}
	return EaseQuadOut(time, 0.0f, 1.0f, kMenuFadeInDuration);
}

bool TeamHasUnits(const Registry &registry, TeamId teamID);
