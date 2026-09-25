#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include <RmlUi/Core/EventListener.h>

#include "AICommander.h"
#include "Art.h"
#include "Event.h"
#include "Hotkeys.h"
#include "MenuScreens.h"
#include "PlayingInput.h"
#include "Production.h"
#include "Registry.h"
#include "ResourceSystem.h"
#include "Simulation.h"

namespace Rml {
class Context;
class Element;
class ElementDocument;
class Event;
}

class RmlUiHost;

// Input scope: mouse only (move/buttons, no wheel/keys/text) — forwarding
// keys would double-fire shortcuts (e.g. Space on a focused button + Halt).
class RmlUiHud : public Rml::EventListener
{
public:
    RmlUiHud(Registry &registry, ResourceSystem &resources, ProductionQueue &queue,
             Simulation &sim, HotkeyMap &hotkeys, PlayingInput &playingInput,
             AICommander &ai, const AIDifficulty &difficulty, const bool &showHints,
             bool &playerAutoRepair, float &autoRepairCap, MenuFlow &menu, Art &art,
             EventDispatcher &events, std::function<void()> quitToMenu,
             std::function<void(MenuState)> beginRemap);
    RmlUiHud(const RmlUiHud &) = delete;
    RmlUiHud &operator=(const RmlUiHud &) = delete;

    // Loads hud/pause/outcome/confirm documents and attaches listeners.
    // False when the host is not ready or a document is missing (caller keeps
    // the raygui panels and overlays).
    bool Init(RmlUiHost &host, const std::string &dataDir);
    void Shutdown();
    bool IsReady() const { return ready_; }
    void Hide();

    // First-refusal gate for PlayingInput (Phase 4 preview): true while the
    // pointer is over an interactive control, latched while its mouse button
    // is held. Hover state lags one frame behind the pump in Draw, which is
    // exact for clicks (pointer stationary) and negligible otherwise.
    bool IsPointerOverUI();

    // One in-world frame: mouse pump (skipped in HotkeyRemap — the remap
    // overlay owns the pump there, see RmlUiMenus::DrawRemapOverlay),
    // cached refresh, doc sync, host sync. Called inside BeginDrawing; the
    // host renders with the frame. Returns the confirm-modal button pressed.
    ConfirmChoice Draw(int screenWidth, int screenHeight, float uiScale);

    void ProcessEvent(Rml::Event &event) override;

private:
    void RefreshHud();
    std::string SelectionLine() const;
    std::string SlotsLine() const;
    void RefreshHints();
    void RefreshPause();
    void RefreshOutcome();
    void RefreshConfirm();
    void OnClick(const Rml::String &id);
    void OnChange(Rml::Element *target, const Rml::String &id);
    void Announce(EventType type);

    static void SetDisabled(Rml::Element *el, bool disabled);
    static float Clamp(float value, float lo, float hi);
    static float ReadRange(Rml::Element *el);
    static bool IsInteractiveTag(const Rml::String &tag);
    void SetTextCached(const char *id, const std::string &text);
    void SetRangeIn(const char *id, float value);
    void SetCheckIn(const char *id, bool checked);
    void SetRangeIn(Rml::ElementDocument *doc, const char *id, float value);
    void SetCheckIn(Rml::ElementDocument *doc, const char *id, bool checked);
    void SetTextIn(Rml::ElementDocument *doc, const char *id, const std::string &text);

    Registry &registry_;
    ResourceSystem &resources_;
    ProductionQueue &queue_;
    Simulation &sim_;
    HotkeyMap &hotkeys_;
    PlayingInput &playingInput_;
    AICommander &ai_;
    const AIDifficulty &difficulty_;
    const bool &showHints_;
    bool &playerAutoRepair_;
    float &autoRepairCap_;
    MenuFlow &menu_;
    Art &art_;
    EventDispatcher &events_;
    std::function<void()> quitToMenu_;
    std::function<void(MenuState)> beginRemap_;
    RmlUiHost *host_ = nullptr;
    Rml::Context *context_ = nullptr;
    Rml::ElementDocument *hudDoc_ = nullptr;
    Rml::ElementDocument *pauseDoc_ = nullptr;
    Rml::ElementDocument *outcomeDoc_ = nullptr;
    Rml::ElementDocument *confirmDoc_ = nullptr;
    bool ready_ = false;
    bool dragLatch_ = false;
    ConfirmChoice pendingChoice_ = ConfirmChoice::None;
    std::unordered_map<std::string, std::string> textCache_;
    std::string hintsCache_;
};
