#include "RmlUiMenus.h"

#include <cstdio>
#include <filesystem>
#include <optional>

#include "raygui.h"
#include "raylib.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <RmlUi/Core/Input.h>

#include "Art.h"
#include "Audio.h"
#include "Hotkeys.h"
#include "MapFile.h"
#include "Menu.h"
#include "RmlUiHost.h"
#include "SaveGame.h"

namespace {

int RmlModifiers()
{
    int modifiers = 0;
    if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL))
    {
        modifiers |= Rml::Input::KM_CTRL;
    }
    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
    {
        modifiers |= Rml::Input::KM_SHIFT;
    }
    if (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT))
    {
        modifiers |= Rml::Input::KM_ALT;
    }
    return modifiers;
}

Rml::Input::KeyIdentifier RaylibKeyToRml(int key)
{
    switch (key)
    {
    case KEY_ENTER:
    case KEY_KP_ENTER:
        return Rml::Input::KI_RETURN;
    case KEY_BACKSPACE:
        return Rml::Input::KI_BACK;
    case KEY_DELETE:
        return Rml::Input::KI_DELETE;
    case KEY_LEFT:
        return Rml::Input::KI_LEFT;
    case KEY_RIGHT:
        return Rml::Input::KI_RIGHT;
    case KEY_UP:
        return Rml::Input::KI_UP;
    case KEY_DOWN:
        return Rml::Input::KI_DOWN;
    case KEY_HOME:
        return Rml::Input::KI_HOME;
    case KEY_END:
        return Rml::Input::KI_END;
    case KEY_TAB:
        return Rml::Input::KI_TAB;
    case KEY_LEFT_SHIFT:
        return Rml::Input::KI_LSHIFT;
    case KEY_RIGHT_SHIFT:
        return Rml::Input::KI_RSHIFT;
    case KEY_LEFT_CONTROL:
        return Rml::Input::KI_LCONTROL;
    case KEY_RIGHT_CONTROL:
        return Rml::Input::KI_RCONTROL;
    case KEY_LEFT_ALT:
        return Rml::Input::KI_LMENU;
    case KEY_RIGHT_ALT:
        return Rml::Input::KI_RMENU;
    default:
        break;
    }
    return Rml::Input::KI_UNKNOWN;
}

// Watched keys forwarded to RmlUi. Escape is deliberately absent: the game
// owns Esc through ShortcutBindings (Back action), so forwarding it would
// double-handle every press.
const int kWatchedKeys[] = {
    KEY_ENTER,
    KEY_KP_ENTER,
    KEY_BACKSPACE,
    KEY_DELETE,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_UP,
    KEY_DOWN,
    KEY_HOME,
    KEY_END,
    KEY_TAB,
    KEY_LEFT_SHIFT,
    KEY_RIGHT_SHIFT,
    KEY_LEFT_CONTROL,
    KEY_RIGHT_CONTROL,
    KEY_LEFT_ALT,
    KEY_RIGHT_ALT,
};

// Mouse-only pump for the remap screen: capture polls GetKeyPressed
// directly (mirroring the raygui screen), so keys must not also reach RmlUi
// or a rebound Space/Enter would fire the focused button too.
void PumpMouse(Rml::Context *context)
{
    const int modifiers = RmlModifiers();
    const Vector2 mouse = GetMousePosition();
    context->ProcessMouseMove(static_cast<int>(mouse.x), static_cast<int>(mouse.y), modifiers);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        context->ProcessMouseButtonDown(0, modifiers);
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
    {
        context->ProcessMouseButtonUp(0, modifiers);
    }
}

void PumpInput(Rml::Context *context)
{
    const int modifiers = RmlModifiers();
    const Vector2 mouse = GetMousePosition();
    context->ProcessMouseMove(static_cast<int>(mouse.x), static_cast<int>(mouse.y), modifiers);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        context->ProcessMouseButtonDown(0, modifiers);
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
    {
        context->ProcessMouseButtonUp(0, modifiers);
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
    {
        context->ProcessMouseButtonDown(1, modifiers);
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT))
    {
        context->ProcessMouseButtonUp(1, modifiers);
    }
    const float wheel = GetMouseWheelMove();
    if (wheel != 0.0f)
    {
        context->ProcessMouseWheel(-wheel, modifiers);
    }
    for (const int key : kWatchedKeys)
    {
        if (IsKeyPressed(key))
        {
            context->ProcessKeyDown(RaylibKeyToRml(key), modifiers);
        }
        if (IsKeyReleased(key))
        {
            context->ProcessKeyUp(RaylibKeyToRml(key), modifiers);
        }
    }
    int codepoint = 0;
    while ((codepoint = GetCharPressed()) != 0)
    {
        context->ProcessTextInput(static_cast<Rml::Character>(codepoint));
    }
}

} // namespace

RmlUiMenus::RmlUiMenus(MenuFlow &menu, HotkeyMap &hotkeys, Art &art, Audio &audio,
                       EventDispatcher &events, MenuScreens &screens,
                       MenuCallbacks callbacks)
    : menu_(menu)
    , hotkeys_(hotkeys)
    , art_(art)
    , audio_(audio)
    , events_(events)
    , screens_(screens)
    , callbacks_(std::move(callbacks))
{
}

bool RmlUiMenus::Init(RmlUiHost &host, const std::string &dataDir)
{
    if (ready_)
    {
        return true;
    }
    context_ = host.context();
    if (context_ == nullptr)
    {
        return false;
    }
    dataDir_ = dataDir;
    host_ = &host;
    menuDoc_ = Load("main_menu.rml");
    setupDoc_ = Load("skirmish_setup.rml");
    settingsDoc_ = Load("settings.rml");
    loadDoc_ = Load("load_game.rml");
    confirmDoc_ = Load("confirm.rml");
    remapDoc_ = Load("hotkey_remap.rml");
    if (menuDoc_ == nullptr || setupDoc_ == nullptr || settingsDoc_ == nullptr ||
        loadDoc_ == nullptr || confirmDoc_ == nullptr || remapDoc_ == nullptr)
    {
        Shutdown();
        return false;
    }
    Listen(menuDoc_, "click", { "btn-setup", "btn-load", "btn-settings", "btn-editor",
                                "btn-quit" });
    Listen(setupDoc_, "click",
           { "btn-start", "btn-back-right", "diff-0", "diff-1", "diff-2" });
    Listen(settingsDoc_, "click", { "btn-settings-back", "btn-remap" });
    Listen(settingsDoc_, "change",
           { "opt-camspeed", "opt-minimap", "opt-hints", "opt-master", "opt-music", "opt-sfx",
             "opt-mute", "opt-rightdrag", "opt-colorblind", "opt-uiscale" });
    Listen(loadDoc_, "click",
           { "slot-0", "slot-1", "slot-2", "slot-3", "btn-load-back", "btn-replay" });
    Listen(confirmDoc_, "click", { "btn-yes", "btn-no" });
    Listen(remapDoc_, "click", { "btn-remap-back" });
    PopulateMaps();
    ready_ = true;
    ShowState();
    return true;
}

void RmlUiMenus::Shutdown()
{
    if (context_ != nullptr)
    {
        for (Rml::ElementDocument *doc :
             { menuDoc_, setupDoc_, settingsDoc_, loadDoc_, confirmDoc_, remapDoc_ })
        {
            if (doc != nullptr)
            {
                context_->UnloadDocument(doc);
            }
        }
        context_->Update();
    }
    menuDoc_ = setupDoc_ = settingsDoc_ = loadDoc_ = confirmDoc_ = remapDoc_ = nullptr;
    context_ = nullptr;
    host_ = nullptr;
    ready_ = false;
    pendingChoice_ = ConfirmChoice::None;
}

bool RmlUiMenus::HandlesState() const
{
    if (!ready_)
    {
        return false;
    }
    switch (menu_.state)
    {
    case MenuState::MainMenu:
    case MenuState::SkirmishSetup:
    case MenuState::Settings:
    case MenuState::LoadGame:
    case MenuState::HotkeyRemap:
        return true;
    default:
        return false;
    }
}

void RmlUiMenus::HideAll()
{
    shown_ = false;
    for (Rml::ElementDocument *doc :
         { menuDoc_, setupDoc_, settingsDoc_, loadDoc_, confirmDoc_, remapDoc_ })
    {
        if (doc != nullptr)
        {
            doc->Hide();
        }
    }
}

ConfirmChoice RmlUiMenus::Draw(int screenWidth, int screenHeight)
{
    if (menu_.state == MenuState::HotkeyRemap)
    {
        DrawRemapScreen(screenWidth, screenHeight);
        return ConfirmChoice::None;
    }
    // Audio parity with MenuScreens::Draw (menu branch owns music here).
    audio_.ApplySettings(menu_.settings.masterVolume, menu_.settings.musicVolume,
                         menu_.settings.sfxVolume, menu_.settings.mute);
    audio_.UpdateMusic();
    PumpInput(context_);
    if (!shown_ || menu_.state != shownState_ || menu_.ConfirmOpen() != shownConfirm_)
    {
        ShowState();
    }
    host_->BeginFrame(screenWidth, screenHeight, menu_.settings.uiScale);
    BeginDrawing();
    ClearBackground(RAYWHITE);
    host_->Render();
    EndDrawing();
    const ConfirmChoice out = pendingChoice_;
    pendingChoice_ = ConfirmChoice::None;
    return out;
}

void RmlUiMenus::ProcessEvent(Rml::Event &event)
{
    Rml::Element *target = event.GetTargetElement();
    if (target == nullptr)
    {
        return;
    }
    const Rml::String type = event.GetType();
    const Rml::String id = target->GetId();
    if (type == "click")
    {
        OnClick(id);
    }
    else if (type == "change")
    {
        OnChange(target, id);
    }
}

Rml::ElementDocument *RmlUiMenus::Load(const std::string &file)
{
    return context_->LoadDocument(dataDir_ + "/" + file);
}

void RmlUiMenus::Listen(Rml::ElementDocument *doc, const char *event,
                        std::initializer_list<const char *> ids)
{
    for (const char *id : ids)
    {
        if (Rml::Element *el = doc->GetElementById(id))
        {
            el->AddEventListener(event, this);
        }
    }
}

void RmlUiMenus::BeginRemap(MenuState returnTo)
{
    remapArming_ = -1;
    remapConflictAction_.clear();
    remapReturn_ = returnTo;
    Announce(EventType::MenuAction);
    menu_.state = MenuState::HotkeyRemap;
}

bool RmlUiMenus::CancelRemapCapture()
{
    if (!ready_ || menu_.state != MenuState::HotkeyRemap)
    {
        return false;
    }
    if (remapArming_ >= 0)
    {
        remapArming_ = -1;
    }
    else
    {
        remapConflictAction_.clear();
        menu_.state = remapReturn_;
    }
    return true;
}

void RmlUiMenus::DrawRemapScreen(int screenWidth, int screenHeight)
{
    PumpMouse(context_);
    PollRemapCapture();
    HideAll();
    remapDoc_->Show();
    RefreshRemap();
    host_->BeginFrame(screenWidth, screenHeight, menu_.settings.uiScale);
    BeginDrawing();
    ClearBackground(RAYWHITE);
    host_->Render();
    EndDrawing();
}

void RmlUiMenus::DrawRemapOverlay()
{
    // World-branch use: the caller owns BeginDrawing and the HUD already
    // pumped the mouse this frame, so only poll + sync here.
    PollRemapCapture();
    HideAll();
    remapDoc_->Show();
    RefreshRemap();
}

void RmlUiMenus::RefreshRemap()
{
    const std::string signature = RemapSignature();
    if (signature != remapCache_)
    {
        remapCache_ = signature;
        PopulateRemapRows();
    }
    if (remapArming_ >= 0)
    {
        SetTextIn(remapDoc_, "remap-status", "Press a key for the armed action (Esc cancels)");
    }
    else if (!remapConflictAction_.empty())
    {
        const std::optional<std::string> owner = hotkeys_.ActionForKey(remapConflictKey_);
        char text[128];
        snprintf(text, sizeof(text), "'%s' already fires '%s' - click the row again to steal it.",
                 GetKeyName(remapConflictKey_), owner.has_value() ? owner->c_str() : "?");
        SetTextIn(remapDoc_, "remap-status", text);
    }
    else
    {
        SetTextIn(remapDoc_, "remap-status",
                  "Click a key to rebind it. Digits/Alt (Tier 2) are fixed.");
    }
}

std::string RmlUiMenus::RemapSignature() const
{
    std::string signature;
    for (int i = 0; i < NumHotkeyDefs(); ++i)
    {
        signature += kHotkeyDefs[i].action;
        signature += '=';
        signature += std::to_string(hotkeys_.KeyFor(kHotkeyDefs[i].action));
        signature += ';';
    }
    signature += "armed=" + std::to_string(remapArming_);
    signature += ";conflict=" + remapConflictAction_;
    return signature;
}

void RmlUiMenus::PopulateRemapRows()
{
    const int defCount = NumHotkeyDefs();
    const int perCol = (defCount + 1) / 2;
    Rml::Element *cols[2] = {
        remapDoc_->GetElementById("hkcol-a"),
        remapDoc_->GetElementById("hkcol-b"),
    };
    for (int col = 0; col < 2; ++col)
    {
        while (Rml::Element *child = cols[col]->GetFirstChild())
        {
            cols[col]->RemoveChild(child);
        }
    }
    for (int i = 0; i < defCount; ++i)
    {
        const HotkeyDef &def = kHotkeyDefs[i];
        Rml::ElementPtr row(remapDoc_->CreateElement("div"));
        char rowId[32];
        snprintf(rowId, sizeof(rowId), "hkrow-%d", i);
        row->SetAttribute("id", Rml::String(rowId));
        row->SetAttribute("class", Rml::String("hkrow"));
        if (remapArming_ == i)
        {
            row->SetClass("armed", true);
        }
        Rml::ElementPtr label(remapDoc_->CreateElement("span"));
        label->SetAttribute("class", Rml::String("hklabel"));
        label->SetInnerRML(def.label);
        Rml::ElementPtr key(remapDoc_->CreateElement("button"));
        char keyId[32];
        snprintf(keyId, sizeof(keyId), "hkkey-%d", i);
        key->SetAttribute("id", Rml::String(keyId));
        key->SetAttribute("class", Rml::String("hkkey"));
        const int effective = hotkeys_.KeyFor(def.action);
        std::string keyText = effective <= 0 ? "-" : GetKeyName(effective);
        if (def.chord)
        {
            keyText = "Shift+" + keyText;
        }
        if (remapArming_ == i)
        {
            keyText = "press a key...";
        }
        key->SetInnerRML(keyText.c_str());
        key->AddEventListener("click", this);
        Rml::Element *rowRaw = row.get();
        rowRaw->AppendChild(std::move(label));
        rowRaw->AppendChild(std::move(key));
        cols[i / perCol]->AppendChild(std::move(row));
    }
}

void RmlUiMenus::PollRemapCapture()
{
    if (remapArming_ < 0)
    {
        return;
    }
    // Mirror the raygui capture: Esc/modifiers never bind; the queue drains
    // here after input_.Update already ran (shortcuts processed first).
    int pressed = 0;
    int candidate = 0;
    while ((pressed = GetKeyPressed()) != 0)
    {
        if (pressed != KEY_ESCAPE && pressed != KEY_LEFT_SHIFT &&
            pressed != KEY_RIGHT_SHIFT && pressed != KEY_LEFT_CONTROL &&
            pressed != KEY_RIGHT_CONTROL && pressed != KEY_LEFT_ALT &&
            pressed != KEY_RIGHT_ALT)
        {
            candidate = pressed;
            break;
        }
    }
    if (candidate == 0)
    {
        return;
    }
    const std::string action = kHotkeyDefs[remapArming_].action;
    const std::optional<std::string> owner = hotkeys_.ActionForKey(candidate);
    if (owner.has_value() && *owner != action)
    {
        remapConflictAction_ = action;
        remapConflictKey_ = candidate;
    }
    else
    {
        hotkeys_.Rebind(action, candidate);
        callbacks_.hotkeysRebound();
        menu_.settings.hotkeyOverrides = hotkeys_.Overrides();
        SaveSettings(menu_.settings, kSettingsPath);
        remapArming_ = -1;
    }
}

void RmlUiMenus::ShowState()
{
    HideAll();
    if (Rml::ElementDocument *doc = DocForState(menu_.state))
    {
        doc->Show();
    }
    if (menu_.ConfirmOpen())
    {
        confirmDoc_->Show(Rml::ModalFlag::Modal);
    }
    RefreshCurrent();
    shown_ = true;
    shownState_ = menu_.state;
    shownConfirm_ = menu_.ConfirmOpen();
}

Rml::ElementDocument *RmlUiMenus::DocForState(MenuState state)
{
    switch (state)
    {
    case MenuState::MainMenu:
        return menuDoc_;
    case MenuState::SkirmishSetup:
        return setupDoc_;
    case MenuState::Settings:
        return settingsDoc_;
    case MenuState::LoadGame:
        return loadDoc_;
    case MenuState::HotkeyRemap:
        return remapDoc_;
    default:
        return nullptr;
    }
}

void RmlUiMenus::RefreshCurrent()
{
    switch (menu_.state)
    {
    case MenuState::SkirmishSetup:
        RefreshSetup();
        break;
    case MenuState::Settings:
        RefreshSettings();
        break;
    case MenuState::LoadGame:
        RefreshLoad();
        break;
    case MenuState::HotkeyRemap:
        RefreshRemap();
        break;
    default:
        break;
    }
    if (menu_.ConfirmOpen())
    {
        RefreshConfirm();
    }
}

void RmlUiMenus::Announce(EventType type)
{
    Event bare;
    bare.type = type;
    events_.Dispatch(bare);
}

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

void RmlUiMenus::SetDisabled(Rml::Element *el, bool disabled)
{
    el->SetClass("disabled", disabled);
}

float RmlUiMenus::Clamp(float value, float lo, float hi)
{
    return value < lo ? lo : (value > hi ? hi : value);
}

float RmlUiMenus::ReadRange(Rml::Element *el)
{
    if (auto *control = dynamic_cast<Rml::ElementFormControl *>(el))
    {
        try
        {
            return std::stof(control->GetValue().c_str());
        }
        catch (const std::exception &)
        {
        }
    }
    return 0.0f;
}

void RmlUiMenus::SetRangeIn(Rml::ElementDocument *doc, const char *id, float value)
{
    if (Rml::Element *el = doc->GetElementById(id))
    {
        if (auto *control = dynamic_cast<Rml::ElementFormControl *>(el))
        {
            char text[32];
            snprintf(text, sizeof(text), "%g", static_cast<double>(value));
            control->SetValue(text);
        }
    }
}

void RmlUiMenus::SetCheckIn(Rml::ElementDocument *doc, const char *id, bool checked)
{
    if (Rml::Element *el = doc->GetElementById(id))
    {
        if (checked)
        {
            el->SetAttribute("checked", Rml::String("checked"));
        }
        else
        {
            el->RemoveAttribute("checked");
        }
    }
}

void RmlUiMenus::SetTextIn(Rml::ElementDocument *doc, const char *id, const std::string &text)
{
    if (Rml::Element *el = doc->GetElementById(id))
    {
        el->SetInnerRML(text.c_str());
    }
}
