#include "app/ui/RmlUiMenus.h"

#include <cstdio>

#include "raylib.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <RmlUi/Core/Input.h>

#include "app/data/Audio.h"
#include "app/match/Menu.h"
#include "app/ui/RmlUiHost.h"

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
