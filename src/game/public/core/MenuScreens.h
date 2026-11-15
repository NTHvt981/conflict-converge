#pragma once

#include <functional>
#include <string>

#include "units/AICommander.h"
#include "app/data/Art.h"
#include "app/data/Audio.h"
#include "core/Event.h"
#include "app/input/Hotkeys.h"
#include "app/input/InputManager.h"
#include "world/MapFile.h"
#include "app/match/Menu.h"

struct MenuCallbacks
{
	std::function<void(const std::string &mapPath, AIDifficulty difficulty)> startMatch;
	std::function<void(const std::string &slotPath)> loadSlot;
	std::function<void()> watchReplay;
	std::function<void()> hotkeysRebound;
};

enum class ConfirmChoice
{
	None,
	Yes,
	No
};

class MenuScreens
{
public:
	MenuScreens(MenuFlow &menu, Art &art, Audio &audio, InputManager &input,
				HotkeyMap &hotkeys, EventDispatcher &events, MenuCallbacks callbacks);
	MenuScreens(const MenuScreens &) = delete;
	MenuScreens &operator=(const MenuScreens &) = delete;

	ConfirmChoice Draw(int screenWidth, int screenHeight, float menuStateTime);
	// Fresh scratch canvas; never the live match map.
	void OpenEditor();
	void ApplyHotkeyOverrides();
	void SyncHotkeySettings();

private:
	MenuFlow &menu_;
	Art &art_;
	Audio &audio_;
	InputManager &input_;
	HotkeyMap &hotkeys_;
	EventDispatcher &events_;
	MenuCallbacks callbacks_;
	MapData editorMap_;
	char editorBrush_ = '.';
	std::string editorStatus_;
	char editorSaveName_[64] = "custom";
	bool editorSaveAsOpen_ = false;
	int editorSaveAsBtn_ = 0;
};
