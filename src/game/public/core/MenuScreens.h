#pragma once

#include <functional>
#include <string>

#include "AICommander.h"
#include "Art.h"
#include "Audio.h"
#include "Event.h"
#include "Hotkeys.h"
#include "InputManager.h"
#include "MapFile.h"
#include "Menu.h"

// World-transition callbacks: MenuScreens owns every menu pixel, Game owns
// every world mutation.
struct MenuCallbacks
{
    std::function<void(const std::string &mapPath, AIDifficulty difficulty)> startMatch;
    std::function<void(const std::string &slotPath)> loadSlot;
    std::function<void()> watchReplay;
    // Re-run BindShortcuts after a remap lands (bindings live in Game).
    std::function<void()> hotkeysRebound;
};

enum class ConfirmChoice
{
    None,
    Yes,
    No
};

// MenuScreens owns the Map Editor branch (raygui scratch-canvas painter;
// menus/HUD are RmlUi in RmlUiMenus/RmlUiHud). Needs a window + GL context;
// not headless-testable.
class MenuScreens
{
public:
    MenuScreens(MenuFlow &menu, Art &art, Audio &audio, InputManager &input,
                HotkeyMap &hotkeys, EventDispatcher &events, MenuCallbacks callbacks);
    MenuScreens(const MenuScreens &) = delete;
    MenuScreens &operator=(const MenuScreens &) = delete;

    // Draw the !worldActive Map Editor branch; owns its Begin/EndDrawing
    // pair. Returns the confirm-modal button clicked this frame (None when
    // no modal).
    ConfirmChoice Draw(int screenWidth, int screenHeight, float menuStateTime);
    // Enter the map editor with a fresh scratch canvas (extracted so the
    // RmlUi menu branch can route here too; never the live match map).
    void OpenEditor();
    // Persisted remaps into hotkeys (startup, before BindShortcuts).
    void ApplyHotkeyOverrides();
    // Live overrides back into settings (before every SaveSettings).
    void SyncHotkeySettings();

private:
    MenuFlow &menu_;
    Art &art_;
    Audio &audio_;
    InputManager &input_;
    HotkeyMap &hotkeys_;
    EventDispatcher &events_;
    MenuCallbacks callbacks_;
    MapData editorMap_; // map editor scratch (never the live match)
    char editorBrush_ = '.';
    std::string editorStatus_;
    char editorSaveName_[64] = "custom"; // Save-As buffer (raw char* for raygui)
    bool editorSaveAsOpen_ = false;
    int editorSaveAsBtn_ = 0;
};
