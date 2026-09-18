#pragma once

#include <functional>
#include <string>

#include "AICommander.h" // AIDifficulty for the start-match callback
#include "Art.h"
#include "Audio.h"
#include "Event.h"
#include "Hotkeys.h"
#include "InputManager.h"
#include "MapFile.h" // MapData scratch state + MapEntry setup list
#include "Menu.h"

// World-transition callbacks: MenuScreens owns every menu pixel, Game
// owns every world mutation. The screens never touch the world directly.
struct MenuCallbacks
{
    std::function<void(const std::string &mapPath, AIDifficulty difficulty)> startMatch;
    std::function<void(const std::string &slotPath)> loadSlot;
    std::function<void()> watchReplay;
    // Re-run BindShortcuts after a remap lands (bindings live in Game).
    std::function<void()> hotkeysRebound;
};

// MenuScreens owns the out-of-world menu branch (main/setup/settings/
// remap/load/editor) plus the shared hotkey-remap screen the pause
// overlay borrows. Extracted from Game::Update/DrawHotkeyRemap: menu UI
// state (editor scratch, remap capture, setup scroll) lives here, world
// transitions cross via MenuCallbacks, announcements dispatch directly.
// Needs a window + GL context (raygui/raylib draw calls throughout):
// not headless-testable, the full suite + e2e gate it.
// Non-copyable: reference members bind the owner's storage for life.
class MenuScreens
{
public:
    MenuScreens(MenuFlow &menu, Art &art, Audio &audio, InputManager &input,
                HotkeyMap &hotkeys, EventDispatcher &events, MenuCallbacks callbacks);
    MenuScreens(const MenuScreens &) = delete;
    MenuScreens &operator=(const MenuScreens &) = delete;

    // The !worldActive branch: audio follow, per-screen UI, transition
    // fade. Owns its Begin/EndDrawing pair; the early return stays with
    // the caller. menuStateTime is the Game-owned transition clock.
    void Draw(int screenWidth, int screenHeight, float menuStateTime);
    // Remap screen body, shared by the Settings-chain branch and the
    // pause overlay (same screen, whichever state entered it).
    void DrawRemap(float cx);
    // Arm a remap capture from Settings or pause (records where Esc/Back
    // returns to).
    void BeginRemap(MenuState returnTo);
    // Esc binding: never rebinds — cancels an armed capture, else backs
    // out to wherever the remap screen was entered from. Returns true
    // when the remap screen consumed the key.
    bool CancelRemapCapture();
    // Persisted remaps into hotkeys (startup, before BindShortcuts).
    void ApplyHotkeyOverrides();
    // Live overrides back into settings (before every SaveSettings).
    void SyncHotkeySettings();

private:
    // Bare-event announcer for MenuAction.
    void Announce(EventType type);

    MenuFlow &menu_;
    Art &art_;
    Audio &audio_;
    InputManager &input_;
    HotkeyMap &hotkeys_;
    EventDispatcher &events_;
    MenuCallbacks callbacks_;
    // Setup-screen map list scroll position.
    int setupScroll_ = 0;
    // QoL remap-screen capture state: action index being rebound (-1 =
    // none), pending conflict (action + key awaiting second-click
    // confirm), and where Esc returns to (Settings or Paused).
    int remapArming_ = -1;
    std::string remapConflictAction_;
    int remapConflictKey_ = 0;
    MenuState remapReturn_ = MenuState::Settings;
    // QoL map editor scratch state (24x18 canvas, never the live match).
    MapData editorMap_;
    char editorBrush_ = '.';
    std::string editorStatus_;
    char editorSaveName_[64] = "custom"; // Save-As buffer (raw char* for raygui)
    bool editorSaveAsOpen_ = false;
    int editorSaveAsBtn_ = 0;
};
