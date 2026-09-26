#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "raylib.h"
#include "core/Subsystem.h"

// Fully remappable Tier-1 hotkeys: single source of truth for action ids,
// labels, and default keys. Game::BindShortcuts, the remap screen, and
// settings persistence all read this table.
struct HotkeyDef
{
    const char *action; // stable id used in settings (hotkey.<action>=<key>)
    const char *label;  // remap-screen display text
    int defaultKey;     // raylib key code
    bool chord;         // true = Shift+key via BindChord
};

extern const HotkeyDef kHotkeyDefs[];
int NumHotkeyDefs();
const HotkeyDef *HotkeyDefFor(const std::string &action);
bool IsKnownHotkeyAction(const std::string &action);
// Stable display name for a raylib key code ("A", "F1", "Space", "Esc");
// pure table lookup, safe headless.
const char *HotkeyDisplayName(int key);

class HotkeyMap : public Subsystem
{
public:
    int KeyFor(const std::string &action) const;
    void Rebind(const std::string &action, int newKey);
    void ClearOverrides();
    // Reverse lookup over effective keys (table order wins).
    std::optional<std::string> ActionForKey(int key) const;
    // Overrides only, in table order — what settings persistence saves.
    std::vector<std::pair<std::string, int>> Overrides() const;

private:
    std::unordered_map<std::string, int> overrides_;
};
