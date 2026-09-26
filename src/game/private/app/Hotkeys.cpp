#include "app/Hotkeys.h"

#include <cstdio>

const HotkeyDef kHotkeyDefs[] = {
    { "ToggleHints", "Shortcut hints", KEY_F1, false },
    { "TogglePause", "Pause / resume", KEY_P, false },
    { "Quicksave", "Quicksave", KEY_F5, false },
    { "Quickload", "Quickload", KEY_F9, false },
    { "SaveSlot1", "Save slot 1", KEY_F6, false },
    { "SaveSlot2", "Save slot 2", KEY_F7, false },
    { "SaveSlot3", "Save slot 3", KEY_F8, false },
    { "LoadSlot1", "Load slot 1 (Shift)", KEY_F6, true },
    { "LoadSlot2", "Load slot 2 (Shift)", KEY_F7, true },
    { "LoadSlot3", "Load slot 3 (Shift)", KEY_F8, true },
    { "AttackMove", "Squad attack-move", KEY_A, false },
    { "StanceHold", "Stance: hold", KEY_H, false },
    { "StanceGuard", "Stance: guard", KEY_G, false },
    { "Patrol", "Squad patrol", KEY_V, false },
    { "Rally", "Rally placement", KEY_R, false },
    { "SelectType", "Select all of type", KEY_C, false },
    { "SelectFactories", "Select all factories", KEY_F, false },
    { "SlowestSpeed", "Move at slowest speed", KEY_B, false },
    { "AreaBuild", "Area-build mode", KEY_Z, false },
    { "AreaRepair", "Area-repair mode", KEY_E, false },
    { "AttackGround", "Attack-ground mode", KEY_X, false },
    { "AutoRetreat", "Auto-retreat toggle", KEY_T, false },
    { "JumpPing", "Jump to latest ping", KEY_J, false },
    { "ReplayBack", "Replay: step back", KEY_LEFT, false },
    { "ReplayFwd", "Replay: step forward", KEY_RIGHT, false },
    { "Back", "Back / deselect / cancel", KEY_ESCAPE, false },
    { "Halt", "Halt selected units", KEY_SPACE, false },
};

int NumHotkeyDefs()
{
    return static_cast<int>(sizeof(kHotkeyDefs) / sizeof(kHotkeyDefs[0]));
}

const HotkeyDef *HotkeyDefFor(const std::string &action)
{
    for (int i = 0; i < NumHotkeyDefs(); ++i)
    {
        if (action == kHotkeyDefs[i].action)
        {
            return &kHotkeyDefs[i];
        }
    }
    return nullptr;
}

bool IsKnownHotkeyAction(const std::string &action)
{
    return HotkeyDefFor(action) != nullptr;
}

const char *HotkeyDisplayName(int key)
{
    static char fallback[32];
    if (key >= KEY_A && key <= KEY_Z)
    {
        fallback[0] = static_cast<char>('A' + (key - KEY_A));
        fallback[1] = '\0';
        return fallback;
    }
    if (key >= KEY_ZERO && key <= KEY_NINE)
    {
        fallback[0] = static_cast<char>('0' + (key - KEY_ZERO));
        fallback[1] = '\0';
        return fallback;
    }
    if (key >= KEY_F1 && key <= KEY_F12)
    {
        std::snprintf(fallback, sizeof(fallback), "F%d", key - KEY_F1 + 1);
        return fallback;
    }
    switch (key)
    {
    case KEY_SPACE:
        return "Space";
    case KEY_ESCAPE:
        return "Esc";
    case KEY_ENTER:
        return "Enter";
    case KEY_BACKSPACE:
        return "Backspace";
    case KEY_TAB:
        return "Tab";
    case KEY_LEFT:
        return "Left";
    case KEY_RIGHT:
        return "Right";
    case KEY_UP:
        return "Up";
    case KEY_DOWN:
        return "Down";
    case KEY_LEFT_SHIFT:
    case KEY_RIGHT_SHIFT:
        return "Shift";
    case KEY_LEFT_CONTROL:
    case KEY_RIGHT_CONTROL:
        return "Ctrl";
    case KEY_LEFT_ALT:
    case KEY_RIGHT_ALT:
        return "Alt";
    default:
        break;
    }
    std::snprintf(fallback, sizeof(fallback), "Key%d", key);
    return fallback;
}

int HotkeyMap::KeyFor(const std::string &action) const
{
    const auto override = overrides_.find(action);
    if (override != overrides_.end())
    {
        return override->second;
    }
    const HotkeyDef *def = HotkeyDefFor(action);
    return def != nullptr ? def->defaultKey : 0;
}

void HotkeyMap::Rebind(const std::string &action, int newKey)
{
    if (!IsKnownHotkeyAction(action) || newKey <= 0)
    {
        return;
    }
    overrides_[action] = newKey;
}

void HotkeyMap::ClearOverrides()
{
    overrides_.clear();
}

std::optional<std::string> HotkeyMap::ActionForKey(int key) const
{
    if (key <= 0)
    {
        return std::nullopt;
    }
    for (int i = 0; i < NumHotkeyDefs(); ++i)
    {
        if (KeyFor(kHotkeyDefs[i].action) == key)
        {
            return kHotkeyDefs[i].action;
        }
    }
    return std::nullopt;
}

std::vector<std::pair<std::string, int>> HotkeyMap::Overrides() const
{
    std::vector<std::pair<std::string, int>> out;
    for (int i = 0; i < NumHotkeyDefs(); ++i)
    {
        const auto override = overrides_.find(kHotkeyDefs[i].action);
        if (override != overrides_.end())
        {
            out.emplace_back(override->first, override->second);
        }
    }
    return out;
}
