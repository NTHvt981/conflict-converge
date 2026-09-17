#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "raylib.h" // KEY_* code defaults for the Tier-1 action table

// QoL fully remappable hotkeys (Tier 1: ShortcutRegistry actions only;
// Tier 2 raw-polled digits/Alt are explicitly deferred — see
// plans/HotkeyRemap_Plan.md). Single source of truth for action ids,
// labels, and default keys: Game::BindShortcuts, the remap screen, and
// settings persistence all read this table.
struct HotkeyDef
{
    const char *action; // stable id used in settings (hotkey.<action>=<key>)
    const char *label;  // remap-screen display text
    int defaultKey;     // raylib key code
    bool chord;         // true = Shift+key via BindChord
};

// All 27 Tier-1 BindShortcuts entries, in binding order.
extern const HotkeyDef kHotkeyDefs[];
int NumHotkeyDefs();
const HotkeyDef *HotkeyDefFor(const std::string &action);
bool IsKnownHotkeyAction(const std::string &action);
// Stable display name for a raylib key code ("A", "F1", "Space", "Esc").
// Unlike raylib's GetKeyName (GLFW-backed, layout-dependent, crashes without
// an initialized window), this is a pure table lookup, safe headless.
const char *HotkeyDisplayName(int key);

class HotkeyMap
{
public:
    // Effective key: override if rebound, else the table default; 0 when
    // the action is unknown (never bound — IsKeyPressed(0) can't fire).
    int KeyFor(const std::string &action) const;
    void Rebind(const std::string &action, int newKey); // unknown actions ignored
    void ClearOverrides();
    // Reverse lookup over EFFECTIVE keys (overrides + surviving defaults,
    // table order wins): which action would fire for this key right now.
    // Used for rebind conflict warnings.
    std::optional<std::string> ActionForKey(int key) const;
    // Overrides only, in table order — what settings persistence saves.
    std::vector<std::pair<std::string, int>> Overrides() const;

private:
    std::unordered_map<std::string, int> overrides_;
};
