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

// Draws the shared Esc-confirm modal over the current frame (used by both the
// menu and gameplay branches). Returns the button clicked this frame; the host
// resolves it, since only Game tears the world down.
ConfirmChoice DrawConfirmDialog(const MenuFlow &menu, int screenWidth, int screenHeight);

// MenuScreens owns the out-of-world menu branch (main/setup/settings/
// remap/load/editor) plus the shared hotkey-remap screen the pause overlay
// borrows. Needs a window + GL context; not headless-testable.
class MenuScreens
{
public:
    MenuScreens(MenuFlow &menu, Art &art, Audio &audio, InputManager &input,
                HotkeyMap &hotkeys, EventDispatcher &events, MenuCallbacks callbacks);
    MenuScreens(const MenuScreens &) = delete;
    MenuScreens &operator=(const MenuScreens &) = delete;

    // Draw the !worldActive branch; owns its Begin/EndDrawing pair. Returns
    // the confirm-modal button clicked this frame (None when no modal).
    ConfirmChoice Draw(int screenWidth, int screenHeight, float menuStateTime);
    // Remap screen body, shared by Settings and the pause overlay.
    void DrawRemap(float cx);
    // Arm a remap capture from Settings or pause.
    void BeginRemap(MenuState returnTo);
    // Esc: cancel an armed capture, else back out; true when consumed.
    bool CancelRemapCapture();
    // Persisted remaps into hotkeys (startup, before BindShortcuts).
    void ApplyHotkeyOverrides();
    // Live overrides back into settings (before every SaveSettings).
    void SyncHotkeySettings();

private:
    void Announce(EventType type);

    MenuFlow &menu_;
    Art &art_;
    Audio &audio_;
    InputManager &input_;
    HotkeyMap &hotkeys_;
    EventDispatcher &events_;
    MenuCallbacks callbacks_;
    int setupScroll_ = 0; // setup-screen map list scroll position
    int remapArming_ = -1; // action being rebound (-1 = none)
    std::string remapConflictAction_; // pending conflict (awaiting confirm)
    int remapConflictKey_ = 0;
    MenuState remapReturn_ = MenuState::Settings; // where Esc returns to
    MapData editorMap_; // map editor scratch (never the live match)
    char editorBrush_ = '.';
    std::string editorStatus_;
    char editorSaveName_[64] = "custom"; // Save-As buffer (raw char* for raygui)
    bool editorSaveAsOpen_ = false;
    int editorSaveAsBtn_ = 0;
};
