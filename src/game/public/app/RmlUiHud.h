#pragma once

#include <string>
#include <unordered_map>

#include <RmlUi/Core/EventListener.h>

#include "AICommander.h"
#include "Hotkeys.h"
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

// In-match HUD bound to live state (Phase 3: hud.rml only; pause/outcome/
// confirm overlays stay raygui until Phase 4). Text pushes reuse the pure
// builders from Hud.h; factory/repair/idle controls call the same paths as
// the raygui Draw* panels they replace.
//
// Refresh policy: every Draw pushes changed text only (content cache), so an
// idle match costs ~zero DOM churn. No event subscriptions: the cached
// refresh is cheap enough that dirty flags add nothing.
//
// Input scope: mouse only (move/buttons, no wheel/keys/text). Keyboard stays
// with the game until Phase 4 owns focus routing — forwarding keys would
// double-fire shortcuts (e.g. Space on a focused button + Halt).
class RmlUiHud : public Rml::EventListener
{
public:
    RmlUiHud(Registry &registry, ResourceSystem &resources, ProductionQueue &queue,
             Simulation &sim, HotkeyMap &hotkeys, PlayingInput &playingInput,
             AICommander &ai, const AIDifficulty &difficulty, const bool &showHints,
             bool &playerAutoRepair, float &autoRepairCap);
    RmlUiHud(const RmlUiHud &) = delete;
    RmlUiHud &operator=(const RmlUiHud &) = delete;

    // Loads hud.rml and attaches listeners. False when the host is not ready
    // or the document is missing (caller keeps the raygui panels).
    bool Init(RmlUiHost &host, const std::string &dataDir);
    void Shutdown();
    bool IsReady() const { return ready_; }

    // First-refusal gate for PlayingInput (Phase 4 preview): true while the
    // pointer is over an interactive control, latched while its mouse button
    // is held. Hover state lags one frame behind the pump in Draw, which is
    // exact for clicks (pointer stationary) and negligible otherwise.
    bool IsPointerOverUI();

    // One in-world frame: mouse pump, cached refresh, host sync. Called
    // inside BeginDrawing; the host renders with the frame.
    void Draw(int screenWidth, int screenHeight, float uiScale);

    void ProcessEvent(Rml::Event &event) override;

private:
    void RefreshHud();
    std::string SelectionLine() const;
    std::string SlotsLine() const;
    void RefreshHints();
    void OnClick(const Rml::String &id);
    void OnChange(Rml::Element *target, const Rml::String &id);

    static void SetDisabled(Rml::Element *el, bool disabled);
    static float Clamp(float value, float lo, float hi);
    static float ReadRange(Rml::Element *el);
    static bool IsInteractiveTag(const Rml::String &tag);
    void SetTextCached(const char *id, const std::string &text);
    void SetRangeIn(const char *id, float value);
    void SetCheckIn(const char *id, bool checked);

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
    RmlUiHost *host_ = nullptr;
    Rml::Context *context_ = nullptr;
    Rml::ElementDocument *hudDoc_ = nullptr;
    bool ready_ = false;
    bool dragLatch_ = false;
    std::unordered_map<std::string, std::string> textCache_;
    std::string hintsCache_;
};
