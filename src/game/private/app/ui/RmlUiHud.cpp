#include "app/ui/RmlUiHud.h"

#include <initializer_list>

#include "raylib.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>

#include "app/ui/RmlUiHost.h"

namespace {

// Mouse-only pump (see RmlUiHud docs): wheel/keys/text stay with the game
// until Phase 4 owns focus routing.
void PumpMouse(Rml::Context *context)
{
    const int modifiers = 0;
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
}

} // namespace

RmlUiHud::RmlUiHud(Registry &registry, ResourceSystem &resources, ProductionQueue &queue,
                   Simulation &sim, HotkeyMap &hotkeys, PlayingInput &playingInput,
                   TileMap &map, OccupancyGrid &occ, const bool &showHints,
                   MenuFlow &menu, Art &art, EventDispatcher &events,
                   std::function<void()> quitToMenu,
                   std::function<void(MenuState)> beginRemap)
    : registry_(registry)
    , resources_(resources)
    , queue_(queue)
    , sim_(sim)
    , hotkeys_(hotkeys)
    , playingInput_(playingInput)
    , map_(map)
    , occ_(occ)
    , showHints_(showHints)
    , menu_(menu)
    , art_(art)
    , events_(events)
    , quitToMenu_(std::move(quitToMenu))
    , beginRemap_(std::move(beginRemap))
{
}

bool RmlUiHud::Init(RmlUiHost &host, const std::string &dataDir)
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
    host_ = &host;
    hudDoc_ = context_->LoadDocument(dataDir + "/hud.rml");
    pauseDoc_ = context_->LoadDocument(dataDir + "/pause.rml");
    outcomeDoc_ = context_->LoadDocument(dataDir + "/outcome.rml");
    confirmDoc_ = context_->LoadDocument(dataDir + "/confirm.rml");
    if (hudDoc_ == nullptr || pauseDoc_ == nullptr || outcomeDoc_ == nullptr ||
        confirmDoc_ == nullptr)
    {
        Shutdown();
        return false;
    }
    if (Rml::Element *el = hudDoc_->GetElementById("btn-cancel"))
    {
        el->AddEventListener("click", this);
    }
    auto listen = [this](Rml::ElementDocument *doc, const char *event,
                         std::initializer_list<const char *> ids) {
        for (const char *id : ids)
        {
            if (Rml::Element *el = doc->GetElementById(id))
            {
                el->AddEventListener(event, this);
            }
        }
    };
    listen(hudDoc_, "click",
           { "tab-infantry", "tab-vehicles", "tab-buildings", "place-0", "place-1", "place-2",
             "fac-i-0", "fac-i-1", "fac-i-2", "fac-i-3", "fac-v-0", "fac-v-1", "fac-v-2",
             "fac-v-3", "repr-i-0", "repr-i-1", "repr-i-2", "repr-i-3", "repr-v-0", "repr-v-1",
             "repr-v-2", "repr-v-3", "ab-hold", "ab-guard", "ab-patrol", "ab-attackmove",
             "ab-halt", "ab-repair", "ab-heal", "ab-rally", "ab-demolish" });
    for (Rml::ElementDocument *doc : { pauseDoc_, outcomeDoc_ })
    {
        (void)doc;
    }
    listen(pauseDoc_, "click",
           { "btn-resume", "btn-p-remap", "btn-p-tomenu", "btn-p-todesktop" });
    listen(pauseDoc_, "change",
           { "opt-p-camspeed", "opt-p-minimap", "opt-p-hints", "opt-p-master", "opt-p-music",
             "opt-p-sfx", "opt-p-mute", "opt-p-rightdrag", "opt-p-colorblind" });
    listen(outcomeDoc_, "click", { "btn-o-tomenu", "btn-o-todesktop" });
    listen(confirmDoc_, "click", { "btn-yes", "btn-no" });
    ready_ = true;
    return true;
}

void RmlUiHud::Hide()
{
    if (!ready_)
    {
        return;
    }
    for (Rml::ElementDocument *doc : { hudDoc_, pauseDoc_, outcomeDoc_, confirmDoc_ })
    {
        doc->Hide();
    }
}

void RmlUiHud::Shutdown()
{
    if (context_ != nullptr)
    {
        for (Rml::ElementDocument *doc : { hudDoc_, pauseDoc_, outcomeDoc_, confirmDoc_ })
        {
            if (doc != nullptr)
            {
                context_->UnloadDocument(doc);
            }
        }
        context_->Update();
    }
    hudDoc_ = pauseDoc_ = outcomeDoc_ = confirmDoc_ = nullptr;
    context_ = nullptr;
    host_ = nullptr;
    ready_ = false;
    textCache_.clear();
    hintsCache_.clear();
}

bool RmlUiHud::IsPointerOverUI()
{
    if (!ready_ || context_ == nullptr)
    {
        return false;
    }
    const Rml::Element *hover = context_->GetHoverElement();
    const bool over = hover != nullptr && IsInteractiveTag(hover->GetTagName());
    // Latch while the pressing button is held so slider drags keep owning
    // the pointer even when it leaves the control box.
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
    {
        if (over)
        {
            dragLatch_ = true;
        }
    }
    else
    {
        dragLatch_ = false;
    }
    return over || dragLatch_;
}

ConfirmChoice RmlUiHud::Draw(int screenWidth, int screenHeight, float uiScale)
{
    // The remap overlay owns the mouse pump (see header); pumping here too
    // would double-fire the press edge into two click events.
    if (menu_.state != MenuState::HotkeyRemap)
    {
        PumpMouse(context_);
    }
    RefreshHud();
    for (Rml::ElementDocument *doc : { hudDoc_, pauseDoc_, outcomeDoc_, confirmDoc_ })
    {
        doc->Hide();
    }
    hudDoc_->Show();
    switch (menu_.state)
    {
    case MenuState::Paused:
        pauseDoc_->Show(Rml::ModalFlag::Modal);
        RefreshPause();
        break;
    case MenuState::GameOver:
    case MenuState::Victory:
        outcomeDoc_->Show(Rml::ModalFlag::Modal);
        RefreshOutcome();
        break;
    default:
        break;
    }
    // The modal only ever opens over Playing/Paused.
    if (menu_.ConfirmOpen() &&
        (menu_.state == MenuState::Playing || menu_.state == MenuState::Paused))
    {
        confirmDoc_->Show(Rml::ModalFlag::Modal);
        RefreshConfirm();
    }
    host_->BeginFrame(screenWidth, screenHeight, uiScale);
    const ConfirmChoice out = pendingChoice_;
    pendingChoice_ = ConfirmChoice::None;
    return out;
}

void RmlUiHud::ProcessEvent(Rml::Event &event)
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

void RmlUiHud::SetDisabled(Rml::Element *el, bool disabled)
{
    el->SetClass("disabled", disabled);
}

float RmlUiHud::Clamp(float value, float lo, float hi)
{
    return value < lo ? lo : (value > hi ? hi : value);
}

float RmlUiHud::ReadRange(Rml::Element *el)
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

bool RmlUiHud::IsInteractiveTag(const Rml::String &tag)
{
    return tag == "button" || tag == "input" || tag == "select" || tag == "textarea";
}

void RmlUiHud::SetTextCached(const char *id, const std::string &text)
{
    auto it = textCache_.find(id);
    if (it != textCache_.end() && it->second == text)
    {
        return;
    }
    textCache_[id] = text;
    if (Rml::Element *el = hudDoc_->GetElementById(id))
    {
        el->SetInnerRML(text.c_str());
    }
}

void RmlUiHud::SetRangeIn(Rml::ElementDocument *doc, const char *id, float value)
{
    if (doc == nullptr)
    {
        return;
    }
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

void RmlUiHud::SetCheckIn(Rml::ElementDocument *doc, const char *id, bool checked)
{
    if (doc == nullptr)
    {
        return;
    }
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

void RmlUiHud::SetTextIn(Rml::ElementDocument *doc, const char *id, const std::string &text)
{
    if (doc == nullptr)
    {
        return;
    }
    if (Rml::Element *el = doc->GetElementById(id))
    {
        el->SetInnerRML(text.c_str());
    }
}
