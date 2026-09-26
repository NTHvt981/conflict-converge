
#include "app/ui/RmlUiMenus.h"

#include <cstdio>
#include <cstring>
#include <filesystem>

#include "raygui.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>

#include "app/data/Art.h"
#include "app/input/Hotkeys.h"
#include "world/MapFile.h"
#include "app/match/Menu.h"
#include "app/save/SaveGame.h"

void RmlUiMenus::OnClick(const Rml::String &id)
{
    if (id == "btn-setup")
    {
        menu_.OpenSetup(ListMaps("data/maps"));
        Announce(EventType::MenuAction);
    }
    else if (id == "btn-load")
    {
        menu_.OpenLoad();
        Announce(EventType::MenuAction);
    }
    else if (id == "btn-settings")
    {
        menu_.OpenSettings();
        Announce(EventType::MenuAction);
    }
    else if (id == "btn-editor")
    {
        screens_.OpenEditor();
        Announce(EventType::MenuAction);
    }
    else if (id == "btn-quit")
    {
        menu_.OpenQuitConfirm();
        Announce(EventType::MenuAction);
    }
    else if (id == "btn-start")
    {
        // No announce here, matching the raygui Start path.
        if (const MapEntry *sel = menu_.setup.SelectedMap();
            sel != nullptr && menu_.StartMatch())
        {
            callbacks_.startMatch(sel->path, menu_.setup.difficulty);
        }
    }
    else if (id == "btn-back-right")
    {
        menu_.OpenMainMenu();
        Announce(EventType::MenuAction);
    }
    else if (id == "btn-settings-back")
    {
        menu_.settings.hotkeyOverrides = hotkeys_.Overrides();
        SaveSettings(menu_.settings, kSettingsPath);
        menu_.OpenMainMenu();
        Announce(EventType::MenuAction);
    }
    else if (id == "btn-load-back")
    {
        menu_.OpenMainMenu();
        Announce(EventType::MenuAction);
    }
    else if (id == "btn-remap")
    {
        BeginRemap(MenuState::Settings);
    }
    else if (id == "btn-remap-back")
    {
        remapArming_ = -1;
        remapConflictAction_.clear();
        Announce(EventType::MenuAction);
        menu_.state = remapReturn_;
    }
    else if (id.compare(0, 6, "hkkey-") == 0)
    {
        // Mirror the raygui row button: a second click on the conflict row
        // steals the key, otherwise the click toggles arming.
        const int row = std::atoi(id.c_str() + 6);
        if (row >= 0 && row < NumHotkeyDefs())
        {
            const std::string action = kHotkeyDefs[row].action;
            if (!remapConflictAction_.empty() && remapConflictAction_ == action)
            {
                hotkeys_.Rebind(action, remapConflictKey_);
                callbacks_.hotkeysRebound();
                menu_.settings.hotkeyOverrides = hotkeys_.Overrides();
                SaveSettings(menu_.settings, kSettingsPath);
                remapConflictAction_.clear();
                remapArming_ = -1;
            }
            else
            {
                remapArming_ = (remapArming_ == row) ? -1 : row;
                remapConflictAction_.clear();
            }
        }
    }
    else if (id == "btn-yes")
    {
        pendingChoice_ = ConfirmChoice::Yes;
    }
    else if (id == "btn-no")
    {
        pendingChoice_ = ConfirmChoice::No;
    }
    else if (id == "diff-0")
    {
        menu_.SelectDifficulty(AIDifficulty::Easy);
    }
    else if (id == "diff-1")
    {
        menu_.SelectDifficulty(AIDifficulty::Medium);
    }
    else if (id == "diff-2")
    {
        menu_.SelectDifficulty(AIDifficulty::Hard);
    }
    else if (id.compare(0, 8, "mapitem-") == 0)
    {
        menu_.SelectMap(std::atoi(id.c_str() + 8));
    }
    else if (id.compare(0, 5, "slot-") == 0)
    {
        // No announce here, matching the raygui slot path. The disabled
        // class is visual-only, so re-check filled before loading.
        const int slot = std::atoi(id.c_str() + 5);
        if (slot >= 0 && slot < 4)
        {
            const std::string path = slot == 0 ? "data/quicksave.ccpb" : SaveSlotPath(slot);
            std::error_code ec;
            if (std::filesystem::exists(path, ec) && !ec)
            {
                callbacks_.loadSlot(path);
            }
        }
    }
    else if (id == "btn-replay")
    {
        if (ReplayFrameCount(kReplayDir) > 0)
        {
            callbacks_.watchReplay();
        }
    }
    else
    {
        return;
    }
    ShowState();
}

void RmlUiMenus::OnChange(Rml::Element *target, const Rml::String &id)
{
    // NOTE: never call RefreshSettings() from here. Programmatic
    // SetValue/SetAttribute calls (e.g. checkbox "checked") dispatch
    // Change back into this listener, so a refresh would recurse until
    // the stack overflows. Update settings + the value label only; the
    // control already shows the new value.
    char text[32];
    if (id == "opt-camspeed")
    {
        menu_.settings.cameraSpeed = Clamp(ReadRange(target), 100.0f, 800.0f);
        snprintf(text, sizeof(text), "%g", static_cast<double>(menu_.settings.cameraSpeed));
        SetTextIn(settingsDoc_, "val-camspeed", text);
    }
    else if (id == "opt-master")
    {
        menu_.settings.masterVolume = Clamp(ReadRange(target), 0.0f, 1.0f);
        snprintf(text, sizeof(text), "%g", static_cast<double>(menu_.settings.masterVolume));
        SetTextIn(settingsDoc_, "val-master", text);
    }
    else if (id == "opt-music")
    {
        menu_.settings.musicVolume = Clamp(ReadRange(target), 0.0f, 1.0f);
        snprintf(text, sizeof(text), "%g", static_cast<double>(menu_.settings.musicVolume));
        SetTextIn(settingsDoc_, "val-music", text);
    }
    else if (id == "opt-sfx")
    {
        menu_.settings.sfxVolume = Clamp(ReadRange(target), 0.0f, 1.0f);
        snprintf(text, sizeof(text), "%g", static_cast<double>(menu_.settings.sfxVolume));
        SetTextIn(settingsDoc_, "val-sfx", text);
    }
    else if (id == "opt-uiscale")
    {
        menu_.settings.uiScale = Clamp(ReadRange(target), 0.75f, 2.0f);
        // Keeps the raygui Map Editor legible; the RmlUi ratio follows
        // uiScale every frame via BeginFrame.
        GuiSetStyle(DEFAULT, TEXT_SIZE,
                    static_cast<int>(10 * menu_.settings.uiScale));
        snprintf(text, sizeof(text), "%g", static_cast<double>(menu_.settings.uiScale));
        SetTextIn(settingsDoc_, "val-uiscale", text);
    }
    else if (id == "opt-minimap")
    {
        menu_.settings.showMinimap = target->HasAttribute("checked");
    }
    else if (id == "opt-hints")
    {
        menu_.settings.showHints = target->HasAttribute("checked");
    }
    else if (id == "opt-mute")
    {
        menu_.settings.mute = target->HasAttribute("checked");
    }
    else if (id == "opt-rightdrag")
    {
        menu_.settings.rightDragPan = target->HasAttribute("checked");
    }
    else if (id == "opt-colorblind")
    {
        menu_.settings.colorBlindMode = target->HasAttribute("checked");
        art_.SetColorBlindMode(menu_.settings.colorBlindMode);
    }
    else
    {
        return;
    }
}

void RmlUiMenus::PopulateMaps()
{
    std::string signature;
    for (const MapEntry &entry : menu_.setup.maps)
    {
        signature += entry.path;
        signature += '\n';
    }
    if (signature == mapCache_)
    {
        return;
    }
    mapCache_ = signature;
    Rml::Element *list = setupDoc_->GetElementById("maplist");
    while (Rml::Element *child = list->GetFirstChild())
    {
        list->RemoveChild(child);
    }
    const std::vector<MapEntry> &maps = menu_.setup.maps;
    for (std::size_t i = 0; i < maps.size(); ++i)
    {
        const MapEntry &entry = maps[i];
        Rml::ElementPtr item(setupDoc_->CreateElement("div"));
        char id[32];
        snprintf(id, sizeof(id), "mapitem-%d", static_cast<int>(i));
        item->SetAttribute("id", Rml::String(id));
        item->SetAttribute("class", Rml::String("map-item"));
        char label[128];
        snprintf(label, sizeof(label), "%s (%dx%d)", entry.name.c_str(), entry.width,
                 entry.height);
        item->SetInnerRML(label);
        item->AddEventListener("click", this);
        list->AppendChild(std::move(item));
    }
}

void RmlUiMenus::RefreshSetup()
{
    PopulateMaps();
    for (std::size_t i = 0; i < menu_.setup.maps.size(); ++i)
    {
        char id[32];
        snprintf(id, sizeof(id), "mapitem-%d", static_cast<int>(i));
        if (Rml::Element *el = setupDoc_->GetElementById(id))
        {
            el->SetClass("selected", static_cast<int>(i) == menu_.setup.mapIndex);
        }
    }
    if (Rml::Element *detail = setupDoc_->GetElementById("mapdetail"))
    {
        if (const MapEntry *sel = menu_.setup.SelectedMap())
        {
            char text[256];
            snprintf(text, sizeof(text), "by %s  %s", sel->author.empty() ? "-" : sel->author.c_str(),
                     sel->path.c_str());
            detail->SetInnerRML(text);
        }
        else
        {
            detail->SetInnerRML("");
        }
    }
    const int diff = static_cast<int>(menu_.setup.difficulty);
    for (int i = 0; i < 3; ++i)
    {
        char id[16];
        snprintf(id, sizeof(id), "diff-%d", i);
        if (Rml::Element *el = setupDoc_->GetElementById(id))
        {
            el->SetClass("selected", i == diff);
        }
    }
    // The disabled class is visual-only; the click guard lives in OnClick.
    if (Rml::Element *start = setupDoc_->GetElementById("btn-start"))
    {
        SetDisabled(start, !menu_.setup.CanStart());
    }
}

void RmlUiMenus::RefreshSettings()
{
    SetRangeIn(settingsDoc_, "opt-camspeed", menu_.settings.cameraSpeed);
    SetRangeIn(settingsDoc_, "opt-master", menu_.settings.masterVolume);
    SetRangeIn(settingsDoc_, "opt-music", menu_.settings.musicVolume);
    SetRangeIn(settingsDoc_, "opt-sfx", menu_.settings.sfxVolume);
    SetRangeIn(settingsDoc_, "opt-uiscale", menu_.settings.uiScale);
    SetCheckIn(settingsDoc_, "opt-minimap", menu_.settings.showMinimap);
    SetCheckIn(settingsDoc_, "opt-hints", menu_.settings.showHints);
    SetCheckIn(settingsDoc_, "opt-mute", menu_.settings.mute);
    SetCheckIn(settingsDoc_, "opt-rightdrag", menu_.settings.rightDragPan);
    SetCheckIn(settingsDoc_, "opt-colorblind", menu_.settings.colorBlindMode);
    char text[32];
    snprintf(text, sizeof(text), "%g", static_cast<double>(menu_.settings.cameraSpeed));
    SetTextIn(settingsDoc_, "val-camspeed", text);
    snprintf(text, sizeof(text), "%g", static_cast<double>(menu_.settings.masterVolume));
    SetTextIn(settingsDoc_, "val-master", text);
    snprintf(text, sizeof(text), "%g", static_cast<double>(menu_.settings.musicVolume));
    SetTextIn(settingsDoc_, "val-music", text);
    snprintf(text, sizeof(text), "%g", static_cast<double>(menu_.settings.sfxVolume));
    SetTextIn(settingsDoc_, "val-sfx", text);
    snprintf(text, sizeof(text), "%g", static_cast<double>(menu_.settings.uiScale));
    SetTextIn(settingsDoc_, "val-uiscale", text);
}

void RmlUiMenus::RefreshLoad()
{
    for (int i = 0; i < 4; ++i)
    {
        char id[16];
        snprintf(id, sizeof(id), "slot-%d", i);
        const std::string path = i == 0 ? "data/quicksave.ccpb" : SaveSlotPath(i);
        std::error_code ec;
        const bool filled = std::filesystem::exists(path, ec) && !ec;
        if (Rml::Element *el = loadDoc_->GetElementById(id))
        {
            SetDisabled(el, !filled);
        }
    }
    if (Rml::Element *replay = loadDoc_->GetElementById("btn-replay"))
    {
        SetDisabled(replay, ReplayFrameCount(kReplayDir) <= 0);
    }
}

void RmlUiMenus::RefreshConfirm()
{
    // Texts match the confirm.rml dialog exactly.
    const bool quitting = menu_.confirm == ConfirmKind::QuitApp;
    SetTextIn(confirmDoc_, "confirm-title",
              quitting ? "Quit Conflict Converge?" : "Return to main menu?");
    SetTextIn(confirmDoc_, "confirm-sub",
              quitting ? "Close the game?" : "Abandon the current match?");
}
