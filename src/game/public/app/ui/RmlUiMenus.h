#pragma once

#include <initializer_list>
#include <string>

#include <RmlUi/Core/EventListener.h>

#include "core/MenuScreens.h"

namespace Rml {
class Context;
class Element;
class ElementDocument;
class Event;
}

class Art;
class Audio;
class EventDispatcher;
class HotkeyMap;
class RmlUiHost;
struct MenuFlow;

// Parity notes vs MenuScreens (raygui), kept deliberately:
// - Esc/Back stays with ShortcutBindings (game owns Esc); forwarded keys
//   exclude Escape so RmlUi never double-handles it.
// - Settings write live into MenuSettings but persist to settings.cfg on
//   Back, like the raygui screen (plus the TEXT_SIZE + color-blind live
//   applies the raygui screen does every draw).
// - Clicks that raygui does not announce (Start, load-slot, replay) stay
//   silent here too.
// - HotkeyRemap/MapEditor/ReplayViewer stay with MenuScreens (capture and
//   editor canvas are Phase 4); the remap entry point routes there.
class RmlUiMenus : public Rml::EventListener
{
public:
    RmlUiMenus(MenuFlow &menu, HotkeyMap &hotkeys, Art &art, Audio &audio,
               EventDispatcher &events, MenuScreens &screens, MenuCallbacks callbacks);
    RmlUiMenus(const RmlUiMenus &) = delete;
    RmlUiMenus &operator=(const RmlUiMenus &) = delete;

    // Loads the five menu documents and attaches listeners. Hides the Phase 1
    // stub on success. False when the host is not ready or a document is
    // missing (caller falls back to the raygui branch).
    bool Init(RmlUiHost &host, const std::string &dataDir);
    void Shutdown();
    bool IsReady() const { return ready_; }

    // True for the states this owner draws (caller skips MenuScreens::Draw).
    // False before Init succeeds, so a document failure falls back to the
    // raygui branch instead of dereferencing a null context.
    bool HandlesState() const;
    // Hides every document (call when falling back to another branch).
    void HideAll();
    // One menu-branch frame: audio parity, input pump, doc sync, own
    // Begin/EndDrawing pair. Returns the confirm-modal button pressed.
    // HotkeyRemap routes internally to the remap screen.
    ConfirmChoice Draw(int screenWidth, int screenHeight);
    // World-branch remap overlay (Phase 4): no frame management, no audio —
    // the caller owns BeginDrawing and the HUD pumps the mouse. Shares the
    // capture machine below.
    void DrawRemapOverlay();
    // Menu-branch remap screen: own Begin/EndDrawing pair (called from Draw
    // for HotkeyRemap).
    void DrawRemapScreen(int screenWidth, int screenHeight);
    // Arm a remap capture (mirrors MenuScreens::BeginRemap; RML remap entries
    // from Settings or pause call this instead).
    void BeginRemap(MenuState returnTo);
    // Esc: cancel an armed capture, else back out; true when consumed
    // (mirrors MenuScreens::CancelRemapCapture; ShortcutBindings prefers this
    // while the RML remap screen owns the state).
    bool CancelRemapCapture();

    void ProcessEvent(Rml::Event &event) override;

private:
    Rml::ElementDocument *Load(const std::string &file);
    void Listen(Rml::ElementDocument *doc, const char *event,
                std::initializer_list<const char *> ids);
    void ShowState();
    Rml::ElementDocument *DocForState(MenuState state);
    void RefreshCurrent();
    void RefreshSetup();
    void RefreshSettings();
    void RefreshLoad();
    void RefreshConfirm();
    void PopulateMaps();
    void RefreshRemap();
    void PopulateRemapRows();
    void PollRemapCapture();
    std::string RemapSignature() const;
    void OnClick(const Rml::String &id);
    void OnChange(Rml::Element *target, const Rml::String &id);
    void Announce(EventType type);

    static void SetDisabled(Rml::Element *el, bool disabled);
    static float Clamp(float value, float lo, float hi);
    static float ReadRange(Rml::Element *el);
    void SetRangeIn(Rml::ElementDocument *doc, const char *id, float value);
    void SetCheckIn(Rml::ElementDocument *doc, const char *id, bool checked);
    void SetTextIn(Rml::ElementDocument *doc, const char *id, const std::string &text);

    MenuFlow &menu_;
    HotkeyMap &hotkeys_;
    Art &art_;
    Audio &audio_;
    EventDispatcher &events_;
    MenuScreens &screens_;
    MenuCallbacks callbacks_;
    RmlUiHost *host_ = nullptr;
    Rml::Context *context_ = nullptr;
    std::string dataDir_;
    bool ready_ = false;
    bool shown_ = false;
    MenuState shownState_ = MenuState::MainMenu;
    bool shownConfirm_ = false;
    ConfirmChoice pendingChoice_ = ConfirmChoice::None;
    Rml::ElementDocument *menuDoc_ = nullptr;
    Rml::ElementDocument *setupDoc_ = nullptr;
    Rml::ElementDocument *settingsDoc_ = nullptr;
    Rml::ElementDocument *loadDoc_ = nullptr;
    Rml::ElementDocument *confirmDoc_ = nullptr;
    Rml::ElementDocument *remapDoc_ = nullptr;
    int remapArming_ = -1; // action being rebound (-1 = none)
    std::string remapConflictAction_; // pending conflict (awaiting steal click)
    int remapConflictKey_ = 0;
    MenuState remapReturn_ = MenuState::Settings; // where Back returns to
    std::string remapCache_;
    std::string mapCache_;
};
