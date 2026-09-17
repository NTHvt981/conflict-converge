#include "Menu.h"

#include <fstream>
#include <sstream>

#include "Hotkeys.h" // hotkey.* override validation (known action ids)

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
    std::ofstream file(path, std::ios::trunc);
    if (!file)
    {
        return false;
    }
    file << "# Conflict Converge settings v1\n";
    file << "cameraSpeed=" << settings.cameraSpeed << "\n";
    file << "showMinimap=" << (settings.showMinimap ? 1 : 0) << "\n";
    file << "rightDragPan=" << (settings.rightDragPan ? 1 : 0) << "\n";
    file << "colorBlindMode=" << (settings.colorBlindMode ? 1 : 0) << "\n";
    for (const auto &override : settings.hotkeyOverrides)
    {
        file << "hotkey." << override.first << "=" << override.second << "\n";
    }
    file << "masterVolume=" << settings.masterVolume << "\n";
    file << "musicVolume=" << settings.musicVolume << "\n";
    file << "sfxVolume=" << settings.sfxVolume << "\n";
    file << "mute=" << (settings.mute ? 1 : 0) << "\n";
    return static_cast<bool>(file);
}

bool LoadSettings(MenuSettings &settings, const std::string &path)
{
    std::ifstream file(path);
    if (!file)
    {
        return false;
    }
    MenuSettings parsed = settings; // malformed lines keep current values
    std::string line;
    while (std::getline(file, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if (line.empty() || line[0] == '#')
        {
            continue;
        }
        const std::size_t eq = line.find('=');
        if (eq == std::string::npos)
        {
            continue;
        }
        const std::string key = line.substr(0, eq);
        const std::string value = line.substr(eq + 1);
        float number = 0.0f;
        if (key == "cameraSpeed")
        {
            if (ParseFloat(value, number))
            {
                parsed.cameraSpeed = ClampFloat(number, 100.0f, 800.0f);
            }
        }
        else if (key == "showMinimap")
        {
            if (value == "0" || value == "1")
            {
                parsed.showMinimap = value == "1";
            }
        }
        else if (key == "rightDragPan")
        {
            if (value == "0" || value == "1")
            {
                parsed.rightDragPan = value == "1";
            }
        }
        else if (key == "colorBlindMode")
        {
            if (value == "0" || value == "1")
            {
                parsed.colorBlindMode = value == "1";
            }
        }
        else if (key == "masterVolume")
        {
            if (ParseFloat(value, number))
            {
                parsed.masterVolume = ClampFloat(number, 0.0f, 1.0f);
            }
        }
        else if (key == "musicVolume")
        {
            if (ParseFloat(value, number))
            {
                parsed.musicVolume = ClampFloat(number, 0.0f, 1.0f);
            }
        }
        else if (key == "sfxVolume")
        {
            if (ParseFloat(value, number))
            {
                parsed.sfxVolume = ClampFloat(number, 0.0f, 1.0f);
            }
        }
        else if (key == "mute")
        {
            if (value == "0" || value == "1")
            {
                parsed.mute = value == "1";
            }
        }
        else if (key.rfind("hotkey.", 0) == 0)
        {
            // QoL remap persistence: known action + positive int key only;
            // unknown actions and malformed keys are skipped (same tolerant
            // posture as every other setting above). Later lines win.
            const std::string action = key.substr(7);
            if (IsKnownHotkeyAction(action))
            {
                try
                {
                    const int code = std::stoi(value);
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
    }
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
