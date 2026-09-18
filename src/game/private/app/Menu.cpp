#include "Menu.h"

#include <cstdio> // snprintf for float formatting into rini text values
#include <cstring>
#include <fstream>
#include <sstream>

#include "Hotkeys.h" // hotkey.* override validation (known action ids)
#include "rini.h"    // raysan5/rini settings reader/writer (RINI_VALUE_DELIMITER
                     // '=' is set in src/thirdparty/rini_impl.cpp, the sole
                     // RINI_IMPLEMENTATION TU; the declarations used here don't
                     // depend on it)

void MenuFlow::TogglePause()
{
    if (state == MenuState::Playing)
    {
        state = MenuState::Paused;
    }
    else if (state == MenuState::Paused)
    {
        state = MenuState::Playing;
    }
}

void MenuFlow::ShowOutcome(bool playerAlive, bool enemyAlive)
{
    if (!playerAlive)
    {
        state = MenuState::GameOver;
    }
    else if (!enemyAlive)
    {
        state = MenuState::Victory;
    }
}

void MenuFlow::OpenMainMenu()
{
    state = MenuState::MainMenu;
}

void MenuFlow::OpenSetup(const std::vector<MapEntry> &maps)
{
    setup.maps = maps;
    setup.mapIndex = -1;
    state = MenuState::SkirmishSetup;
}

void MenuFlow::OpenSettings()
{
    state = MenuState::Settings;
}

void MenuFlow::OpenLoad()
{
    state = MenuState::LoadGame;
}

void MenuFlow::SelectMap(int index)
{
    if (index >= 0 && index < static_cast<int>(setup.maps.size()))
    {
        setup.mapIndex = index;
    }
}

void MenuFlow::SelectDifficulty(AIDifficulty difficulty)
{
    setup.difficulty = difficulty;
}

bool MenuFlow::StartMatch()
{
    if (state != MenuState::SkirmishSetup || !setup.CanStart())
    {
        return false;
    }
    state = MenuState::Playing;
    return true;
}

namespace
{

bool ParseFloat(const std::string &text, float &out)
{
    std::istringstream rest(text);
    float value = 0.0f;
    char extra = 0;
    if (!(rest >> value) || (rest >> extra))
    {
        return false;
    }
    out = value;
    return true;
}

float ClampFloat(float value, float lo, float hi)
{
    if (value < lo)
    {
        return lo;
    }
    if (value > hi)
    {
        return hi;
    }
    return value;
}

} // namespace

bool SaveSettings(const MenuSettings &settings, const std::string &path)
{
    // rini-backed writer: scalars as int/text entries, hotkey remaps as
    // individual hotkey.<action> int entries (same key shape as the old
    // hand-rolled format).
    rini_data data = rini_load(NULL); // empty object, RINI_MAX_ENTRY_CAPACITY slots
    char number[64];
    rini_set_comment_line(&data, " Conflict Converge settings v1");
    std::snprintf(number, sizeof(number), "%g", settings.cameraSpeed);
    rini_set_value_text(&data, "cameraSpeed", number, NULL);
    rini_set_value(&data, "showMinimap", settings.showMinimap ? 1 : 0, NULL);
    rini_set_value(&data, "rightDragPan", settings.rightDragPan ? 1 : 0, NULL);
    rini_set_value(&data, "colorBlindMode", settings.colorBlindMode ? 1 : 0, NULL);
    std::snprintf(number, sizeof(number), "%g", settings.uiScale);
    rini_set_value_text(&data, "uiScale", number, NULL);
    for (const auto &override : settings.hotkeyOverrides)
    {
        const std::string key = "hotkey." + override.first;
        rini_set_value(&data, key.c_str(), override.second, NULL);
    }
    std::snprintf(number, sizeof(number), "%g", settings.masterVolume);
    rini_set_value_text(&data, "masterVolume", number, NULL);
    std::snprintf(number, sizeof(number), "%g", settings.musicVolume);
    rini_set_value_text(&data, "musicVolume", number, NULL);
    std::snprintf(number, sizeof(number), "%g", settings.sfxVolume);
    rini_set_value_text(&data, "sfxVolume", number, NULL);
    rini_set_value(&data, "mute", settings.mute ? 1 : 0, NULL);
    rini_save(data, path.c_str());
    rini_unload(&data);
    // rini_save reports no status, so probe the file like the old
    // std::ofstream-truthiness check did (missing dir -> false).
    std::ifstream probe(path);
    return probe.good();
}

namespace
{

// rini's '=' value delimiter only takes effect for spaced pairs: its value
// scanner stops at ' ', so a bare `key=value` (the format every shipped
// settings.cfg uses) overshoots the '=' and parses an empty value (verified
// against the vendored header with a compiled probe: `mute=1` -> text="").
// Rewriting content lines to `key = value` before rini_load_from_memory
// keeps old files readable with stock rini — no fork, survives bootstrap
// re-clones. Comment/empty/delimiter-less lines pass through untouched
// (rini skips them, same as the old loader).
std::string NormalizeSettingsText(const std::string &text)
{
    std::istringstream lines(text);
    std::string out;
    std::string line;
    while (std::getline(lines, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        const std::size_t eq = line.find('=');
        if (!line.empty() && line[0] != '#' && eq != std::string::npos)
        {
            line = line.substr(0, eq) + " = " + line.substr(eq + 1);
        }
        out += line;
        out += '\n';
    }
    return out;
}

// Last-match lookup over the loaded entries (mirrors the old line-by-line
// loader where later lines overwrote earlier ones; rini's own getters
// return the FIRST match). Returns nullptr when the key is absent so
// callers leave the current value untouched.
const char *FindEntryText(const rini_data &data, const char *key)
{
    const char *found = NULL;
    for (unsigned int i = 0; i < data.count; ++i)
    {
        if (std::strcmp(data.entries[i].key, key) == 0)
        {
            found = data.entries[i].text;
        }
    }
    return found;
}

} // namespace

bool LoadSettings(MenuSettings &settings, const std::string &path)
{
    std::ifstream file(path);
    if (!file)
    {
        return false; // missing/unreadable: leave settings untouched
    }
    std::ostringstream raw;
    raw << file.rdbuf();
    const std::string normalized = NormalizeSettingsText(raw.str());
    rini_data data = rini_load_from_memory(normalized.c_str());
    MenuSettings parsed = settings; // malformed values keep current values
    float number = 0.0f;
    const char *text = NULL;
    if ((text = FindEntryText(data, "cameraSpeed")) != NULL)
    {
        if (ParseFloat(text, number))
        {
            parsed.cameraSpeed = ClampFloat(number, 100.0f, 800.0f);
        }
    }
    if ((text = FindEntryText(data, "showMinimap")) != NULL)
    {
        if (std::strcmp(text, "0") == 0 || std::strcmp(text, "1") == 0)
        {
            parsed.showMinimap = std::strcmp(text, "1") == 0;
        }
    }
    if ((text = FindEntryText(data, "rightDragPan")) != NULL)
    {
        if (std::strcmp(text, "0") == 0 || std::strcmp(text, "1") == 0)
        {
            parsed.rightDragPan = std::strcmp(text, "1") == 0;
        }
    }
    if ((text = FindEntryText(data, "colorBlindMode")) != NULL)
    {
        if (std::strcmp(text, "0") == 0 || std::strcmp(text, "1") == 0)
        {
            parsed.colorBlindMode = std::strcmp(text, "1") == 0;
        }
    }
    if ((text = FindEntryText(data, "uiScale")) != NULL)
    {
        if (ParseFloat(text, number))
        {
            parsed.uiScale = ClampFloat(number, 0.75f, 2.0f);
        }
    }
    if ((text = FindEntryText(data, "masterVolume")) != NULL)
    {
        if (ParseFloat(text, number))
        {
            parsed.masterVolume = ClampFloat(number, 0.0f, 1.0f);
        }
    }
    if ((text = FindEntryText(data, "musicVolume")) != NULL)
    {
        if (ParseFloat(text, number))
        {
            parsed.musicVolume = ClampFloat(number, 0.0f, 1.0f);
        }
    }
    if ((text = FindEntryText(data, "sfxVolume")) != NULL)
    {
        if (ParseFloat(text, number))
        {
            parsed.sfxVolume = ClampFloat(number, 0.0f, 1.0f);
        }
    }
    if ((text = FindEntryText(data, "mute")) != NULL)
    {
        if (std::strcmp(text, "0") == 0 || std::strcmp(text, "1") == 0)
        {
            parsed.mute = std::strcmp(text, "1") == 0;
        }
    }
    // QoL remap persistence: scan every entry in file order so later
    // hotkey.<action> lines win, exactly like the old loader. Known action
    // + positive int key only; unknown/malformed entries are skipped.
    for (unsigned int i = 0; i < data.count; ++i)
    {
        const char *key = data.entries[i].key;
        static const char kPrefix[] = "hotkey.";
        if (std::strncmp(key, kPrefix, sizeof(kPrefix) - 1) != 0)
        {
            continue;
        }
        const std::string action = key + sizeof(kPrefix) - 1;
        if (IsKnownHotkeyAction(action))
        {
            try
            {
                const int code = std::stoi(data.entries[i].text);
                if (code > 0)
                {
                    bool replaced = false;
                    for (auto &override : parsed.hotkeyOverrides)
                    {
                        if (override.first == action)
                        {
                            override.second = code;
                            replaced = true;
                        }
                    }
                    if (!replaced)
                    {
                        parsed.hotkeyOverrides.emplace_back(action, code);
                    }
                }
            }
            catch (const std::exception &)
            {
            }
        }
    }
    rini_unload(&data);
    settings = parsed;
    return true;
}

bool TeamHasUnits(const Registry &registry, int teamID)
{
    bool found = false;
    registry.Each<Unit>([&](Entity, const Unit &unit) {
        if (unit.teamID == teamID && unit.health > 0.0f)
        {
            found = true;
        }
    });
    return found;
}
