#include "app/ui/RmlUiHud.h"

#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "raylib.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>

#include "economy/Building.h"
#include "app/ui/Hud.h"
#include "app/ui/RmlUiHost.h"
#include "app/input/Selection.h"
#include "units/UnitCommands.h"
#include "units/UnitFactory.h"

namespace {

// Ability buttons in panel order; ids match hud.rml.
const std::pair<const char *, AbilityId> kAbilityButtons[] = {
    { "ab-hold", AbilityId::Hold },         { "ab-guard", AbilityId::Guard },
    { "ab-patrol", AbilityId::Patrol },     { "ab-attackmove", AbilityId::AttackMove },
    { "ab-halt", AbilityId::Halt },         { "ab-repair", AbilityId::Repair },
    { "ab-heal", AbilityId::Heal },         { "ab-rally", AbilityId::Rally },
    { "ab-demolish", AbilityId::Demolish },
};

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

// Player team for every HUD readout, matching the raygui panels.
constexpr int kPlayerTeam = 0;

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

void RmlUiHud::RefreshPause()
{
    SetRangeIn(pauseDoc_, "opt-p-camspeed", menu_.settings.cameraSpeed);
    SetRangeIn(pauseDoc_, "opt-p-master", menu_.settings.masterVolume);
    SetRangeIn(pauseDoc_, "opt-p-music", menu_.settings.musicVolume);
    SetRangeIn(pauseDoc_, "opt-p-sfx", menu_.settings.sfxVolume);
    SetCheckIn(pauseDoc_, "opt-p-minimap", menu_.settings.showMinimap);
    SetCheckIn(pauseDoc_, "opt-p-hints", menu_.settings.showHints);
    SetCheckIn(pauseDoc_, "opt-p-mute", menu_.settings.mute);
    SetCheckIn(pauseDoc_, "opt-p-rightdrag", menu_.settings.rightDragPan);
    SetCheckIn(pauseDoc_, "opt-p-colorblind", menu_.settings.colorBlindMode);
    char text[32];
    snprintf(text, sizeof(text), "%g", static_cast<double>(menu_.settings.cameraSpeed));
    SetTextIn(pauseDoc_, "val-p-camspeed", text);
    snprintf(text, sizeof(text), "%g", static_cast<double>(menu_.settings.masterVolume));
    SetTextIn(pauseDoc_, "val-p-master", text);
    snprintf(text, sizeof(text), "%g", static_cast<double>(menu_.settings.musicVolume));
    SetTextIn(pauseDoc_, "val-p-music", text);
    snprintf(text, sizeof(text), "%g", static_cast<double>(menu_.settings.sfxVolume));
    SetTextIn(pauseDoc_, "val-p-sfx", text);
}

void RmlUiHud::RefreshOutcome()
{
    const bool won = menu_.state == MenuState::Victory;
    SetTextIn(outcomeDoc_, "outcome-title", won ? "Victory!" : "Defeat");
    SetTextIn(outcomeDoc_, "outcome-body",
              won ? "Enemy force destroyed." : "Your force was destroyed.");
}

void RmlUiHud::RefreshConfirm()
{
    // Texts match the confirm.rml dialog exactly.
    const bool quitting = menu_.confirm == ConfirmKind::QuitApp;
    SetTextIn(confirmDoc_, "confirm-title",
              quitting ? "Quit Conflict Converge?" : "Return to main menu?");
    SetTextIn(confirmDoc_, "confirm-sub",
              quitting ? "Close the game?" : "Abandon the current match?");
}

void RmlUiHud::Announce(EventType type)
{
    Event bare;
    bare.type = type;
    events_.Dispatch(bare);
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

void RmlUiHud::RefreshHud()
{
    SetTextCached("res-line", FormatResources(resources_));
    SetTextCached("sel-line", SelectionLine());

    char label[64];
    const int autoBit = playingInput_.AutoAddGroupBit();
    for (int bit = 0; bit < 10; ++bit)
    {
        const unsigned int mask = 1u << static_cast<unsigned int>(bit);
        int count = 0;
        bool allSelected = true;
        registry_.Each<Unit>([&](Entity, const Unit &unit) {
            if (unit.teamID != kPlayerTeam || (unit.controlGroups & mask) == 0)
            {
                return;
            }
            ++count;
            if (!unit.isSelected)
            {
                allSelected = false;
            }
        });
        char id[16];
        snprintf(id, sizeof(id), "cg-%d", bit);
        snprintf(label, sizeof(label), "%d:%d", (bit + 1) % 10, count);
        SetTextCached(id, label);
        if (Rml::Element *el = hudDoc_->GetElementById(id))
        {
            const bool nonempty = count > 0;
            el->SetClass("nonempty", nonempty);
            el->SetClass("allselected", nonempty && allSelected);
            el->SetClass("autogold", bit == autoBit);
        }
    }

    const bool showRows = sim_.HasFactory();
    if (Rml::Element *stub = hudDoc_->GetElementById("need-factory"))
    {
        stub->SetProperty("display", showRows ? "none" : "block");
    }
    if (Rml::Element *queueRow = hudDoc_->GetElementById("fac-queue-row"))
    {
        queueRow->SetProperty("display", showRows ? "block" : "none");
    }
    RefreshFactory();
    RefreshAbilities();
    snprintf(label, sizeof(label), "Queue: %d", static_cast<int>(queue_.Size()));
    SetTextCached("queue-line", label);

    if (Rml::Element *producing = hudDoc_->GetElementById("hud-producing"))
    {
        producing->SetProperty("display", queue_.Empty() ? "none" : "block");
    }
    if (Rml::Element *fill = hudDoc_->GetElementById("producing-fill"))
    {
        char width[32];
        snprintf(width, sizeof(width), "%g%%",
                 static_cast<double>(queue_.HeadProgress() * 100.0f));
        fill->SetProperty(Rml::String("width"), Rml::String(width));
    }

    snprintf(label, sizeof(label), "%d FPS", GetFPS());
    SetTextCached("fps-line", label);

    if (Rml::Element *hints = hudDoc_->GetElementById("hud-hints"))
    {
        hints->SetProperty("display", showHints_ ? "block" : "none");
    }
    RefreshHints();
}

void RmlUiHud::RefreshFactory()
{
    char label[64];
    const bool showRows = sim_.HasFactory();
    const bool infantryTab = factoryTab_ == ProductionTab::Infantry;
    const bool vehiclesTab = factoryTab_ == ProductionTab::Vehicles;
    const bool buildingsTab = factoryTab_ == ProductionTab::Buildings;
    if (Rml::Element *rows = hudDoc_->GetElementById("rows-infantry"))
    {
        rows->SetProperty("display", showRows && infantryTab ? "block" : "none");
    }
    if (Rml::Element *rows = hudDoc_->GetElementById("rows-vehicles"))
    {
        rows->SetProperty("display", showRows && vehiclesTab ? "block" : "none");
    }
    if (Rml::Element *rows = hudDoc_->GetElementById("rows-buildings"))
    {
        rows->SetProperty("display", buildingsTab ? "block" : "none");
    }
    if (Rml::Element *tab = hudDoc_->GetElementById("tab-infantry"))
    {
        tab->SetClass("selected", infantryTab);
    }
    if (Rml::Element *tab = hudDoc_->GetElementById("tab-vehicles"))
    {
        tab->SetClass("selected", vehiclesTab);
    }
    if (Rml::Element *tab = hudDoc_->GetElementById("tab-buildings"))
    {
        tab->SetClass("selected", buildingsTab);
    }
    const std::vector<UnitType> infantry = InfantryMenuOrder();
    for (std::size_t i = 0; i < infantry.size() && i < 4; ++i)
    {
        const UnitCost cost = CostOf(infantry[i]);
        char id[16];
        snprintf(id, sizeof(id), "fac-i-%d", static_cast<int>(i));
        snprintf(label, sizeof(label), "%s %ld/%ld", UnitTypeName(infantry[i]), cost.iron,
                 cost.oil);
        SetTextCached(id, label);
        if (Rml::Element *el = hudDoc_->GetElementById(id))
        {
            SetDisabled(el, resources_.iron < cost.iron || resources_.oil < cost.oil);
        }
        snprintf(id, sizeof(id), "repr-i-%d", static_cast<int>(i));
        SetTextCached(id, queue_.RepeatArmed(infantry[i]) ? "R*" : "R");
    }
    const std::vector<UnitType> vehicles = VehicleMenuOrder();
    for (std::size_t i = 0; i < vehicles.size() && i < 4; ++i)
    {
        const UnitCost cost = CostOf(vehicles[i]);
        char id[16];
        snprintf(id, sizeof(id), "fac-v-%d", static_cast<int>(i));
        snprintf(label, sizeof(label), "%s %ld/%ld", UnitTypeName(vehicles[i]), cost.iron,
                 cost.oil);
        SetTextCached(id, label);
        if (Rml::Element *el = hudDoc_->GetElementById(id))
        {
            SetDisabled(el, resources_.iron < cost.iron || resources_.oil < cost.oil);
        }
        snprintf(id, sizeof(id), "repr-v-%d", static_cast<int>(i));
        SetTextCached(id, queue_.RepeatArmed(vehicles[i]) ? "R*" : "R");
    }
    const std::vector<BuildingType> buildings = BuildingMenuOrder();
    for (std::size_t i = 0; i < buildings.size() && i < 3; ++i)
    {
        char id[16];
        snprintf(id, sizeof(id), "place-%d", static_cast<int>(i));
        SetTextCached(id, BuildingTypeName(buildings[i]));
    }
    snprintf(label, sizeof(label), "Queue: %d", static_cast<int>(queue_.Size()));
    SetTextCached("queue-line", label);
}

void RmlUiHud::RefreshAbilities()
{
    std::vector<Entity> selection;
    registry_.Each<Unit>([&](Entity id, const Unit &unit) {
        if (unit.isSelected)
        {
            selection.push_back(id);
        }
    });
    registry_.Each<Building>([&](Entity id, const Building &building) {
        if (building.isSelected)
        {
            selection.push_back(id);
        }
    });
    const std::vector<AbilityEntry> entries = AbilitiesForSelection(registry_, selection);
    for (const auto &[buttonId, ability] : kAbilityButtons)
    {
        Rml::Element *el = hudDoc_->GetElementById(buttonId);
        if (el == nullptr)
        {
            continue;
        }
        const AbilityEntry *entry = nullptr;
        for (const AbilityEntry &candidate : entries)
        {
            if (candidate.id == ability)
            {
                entry = &candidate;
                break;
            }
        }
        el->SetProperty("display", entry != nullptr ? "inline-block" : "none");
        if (entry == nullptr)
        {
            continue;
        }
        el->SetInnerRML(AbilityName(ability));
        bool active = entry->active;
        if (ability == AbilityId::Rally && playingInput_.IsSettingRally())
        {
            active = true;
        }
        if (playingInput_.ArmedAbility().has_value() && *playingInput_.ArmedAbility() == ability)
        {
            active = true;
        }
        el->SetClass("active", active);
        SetDisabled(el, !entry->enabled);
    }
}

std::string RmlUiHud::SelectionLine() const
{
    const Entity selected = SelectedUnit(registry_);
    const Unit *unit = registry_.Get<Unit>(selected);
    char buf[128];
    if (unit != nullptr)
    {
        if (SelectedUnitCount(registry_) > 1)
        {
            snprintf(buf, sizeof(buf), "%d units selected", SelectedUnitCount(registry_));
            return buf;
        }
        return SelectionSummary(*unit);
    }
    int count = 0;
    BuildingType firstType = BuildingType::Base;
    registry_.Each<Building>([&](Entity, const Building &building) {
        if (building.isSelected)
        {
            if (count == 0)
            {
                firstType = building.type;
            }
            ++count;
        }
    });
    if (count > 0)
    {
        snprintf(buf, sizeof(buf), "%d %s selected", count, BuildingTypeName(firstType));
        return buf;
    }
    return "No selection";
}

void RmlUiHud::RefreshHints()
{
    const std::vector<std::string> hints = ShortcutHintLines(hotkeys_);
    std::string joined;
    for (const std::string &line : hints)
    {
        joined += line;
        joined += '\n';
    }
    if (joined == hintsCache_)
    {
        return;
    }
    hintsCache_ = joined;
    Rml::Element *list = hudDoc_->GetElementById("hud-hints");
    if (list == nullptr)
    {
        return;
    }
    while (Rml::Element *child = list->GetFirstChild())
    {
        list->RemoveChild(child);
    }
    for (const std::string &line : hints)
    {
        Rml::ElementPtr item(hudDoc_->CreateElement("p"));
        item->SetAttribute("class", Rml::String("hint-line"));
        item->SetInnerRML(line.c_str());
        list->AppendChild(std::move(item));
    }
}

void RmlUiHud::OnClick(const Rml::String &id)
{
    if (id == "btn-cancel")
    {
        queue_.CancelTop(resources_);
        return;
    }
    if (id == "tab-infantry")
    {
        factoryTab_ = ProductionTab::Infantry;
        return;
    }
    if (id == "tab-vehicles")
    {
        factoryTab_ = ProductionTab::Vehicles;
        return;
    }
    if (id == "tab-buildings")
    {
        factoryTab_ = ProductionTab::Buildings;
        return;
    }
    if (id.compare(0, 6, "place-") == 0)
    {
        const int row = std::atoi(id.c_str() + 6);
        const std::vector<BuildingType> order = BuildingMenuOrder();
        if (row >= 0 && row < static_cast<int>(order.size()))
        {
            playingInput_.SelectPlacingType(order[static_cast<std::size_t>(row)]);
        }
        return;
    }
    // Enqueue re-validates affordability (TrySpend); the disabled class
    // is visual-only, matching the raygui panel.
    const std::vector<UnitType> infantry = InfantryMenuOrder();
    const std::vector<UnitType> vehicles = VehicleMenuOrder();
    auto enqueueRow = [&](const char *prefix, const std::vector<UnitType> &order) {
        const std::size_t width = std::strlen(prefix);
        if (id.compare(0, width, prefix) != 0)
        {
            return false;
        }
        const int row = std::atoi(id.c_str() + width);
        if (row >= 0 && row < static_cast<int>(order.size()))
        {
            queue_.Enqueue(resources_, order[static_cast<std::size_t>(row)],
                           queue_.RepeatArmed(order[static_cast<std::size_t>(row)]));
        }
        return true;
    };
    auto repeatRow = [&](const char *prefix, const std::vector<UnitType> &order) {
        const std::size_t width = std::strlen(prefix);
        if (id.compare(0, width, prefix) != 0)
        {
            return false;
        }
        const int row = std::atoi(id.c_str() + width);
        if (row >= 0 && row < static_cast<int>(order.size()))
        {
            const UnitType type = order[static_cast<std::size_t>(row)];
            queue_.SetRepeatArmed(type, !queue_.RepeatArmed(type));
        }
        return true;
    };
    if (enqueueRow("fac-i-", infantry) || enqueueRow("fac-v-", vehicles) ||
        repeatRow("repr-i-", infantry) || repeatRow("repr-v-", vehicles))
    {
        return;
    }
    for (const auto &[buttonId, ability] : kAbilityButtons)
    {
        if (id == buttonId)
        {
            OnAbility(ability);
            return;
        }
    }
    if (id == "btn-resume")
    {
        Announce(EventType::MenuAction);
        menu_.state = MenuState::Playing;
    }
    else if (id == "btn-p-remap")
    {
        beginRemap_(MenuState::Paused);
    }
    else if (id == "btn-p-tomenu")
    {
        quitToMenu_();
    }
    else if (id == "btn-p-todesktop")
    {
        menu_.quitRequested = true;
    }
    else if (id == "btn-o-tomenu")
    {
        quitToMenu_();
    }
    else if (id == "btn-o-todesktop")
    {
        menu_.quitRequested = true;
    }
    else if (id == "btn-yes")
    {
        pendingChoice_ = ConfirmChoice::Yes;
    }
    else if (id == "btn-no")
    {
        pendingChoice_ = ConfirmChoice::No;
    }
}

void RmlUiHud::OnAbility(AbilityId ability)
{
    switch (ability)
    {
    case AbilityId::Hold:
        SetSelectionStance(registry_, Stance::Hold);
        break;
    case AbilityId::Guard:
        SetSelectionStance(registry_, Stance::Guard);
        break;
    case AbilityId::Halt:
        HaltSelection(registry_);
        break;
    case AbilityId::Repair:
        ToggleSelectionAutoRepair(registry_);
        break;
    case AbilityId::Rally:
        playingInput_.ToggleSettingRally();
        break;
    case AbilityId::Demolish:
        registry_.Each<Building>([&](Entity id, const Building &building) {
            if (building.isSelected)
            {
                DemolishBuilding(registry_, map_, id);
            }
        });
        break;
    case AbilityId::AttackMove:
    case AbilityId::Patrol:
    case AbilityId::Heal:
        playingInput_.ArmAbility(ability);
        break;
    }
}

void RmlUiHud::OnChange(Rml::Element *target, const Rml::String &id)
{
    // Same no-refresh rule as the menus: programmatic SetValue/SetAttribute
    // re-dispatches Change into this listener.
    if (id == "opt-p-camspeed")
    {
        menu_.settings.cameraSpeed = Clamp(ReadRange(target), 100.0f, 800.0f);
        char text[32];
        snprintf(text, sizeof(text), "%g", static_cast<double>(menu_.settings.cameraSpeed));
        SetTextIn(pauseDoc_, "val-p-camspeed", text);
    }
    else if (id == "opt-p-master")
    {
        menu_.settings.masterVolume = Clamp(ReadRange(target), 0.0f, 1.0f);
        char text[32];
        snprintf(text, sizeof(text), "%g", static_cast<double>(menu_.settings.masterVolume));
        SetTextIn(pauseDoc_, "val-p-master", text);
    }
    else if (id == "opt-p-music")
    {
        menu_.settings.musicVolume = Clamp(ReadRange(target), 0.0f, 1.0f);
        char text[32];
        snprintf(text, sizeof(text), "%g", static_cast<double>(menu_.settings.musicVolume));
        SetTextIn(pauseDoc_, "val-p-music", text);
    }
    else if (id == "opt-p-sfx")
    {
        menu_.settings.sfxVolume = Clamp(ReadRange(target), 0.0f, 1.0f);
        char text[32];
        snprintf(text, sizeof(text), "%g", static_cast<double>(menu_.settings.sfxVolume));
        SetTextIn(pauseDoc_, "val-p-sfx", text);
    }
    else if (id == "opt-p-minimap")
    {
        menu_.settings.showMinimap = target->HasAttribute("checked");
    }
    else if (id == "opt-p-hints")
    {
        menu_.settings.showHints = target->HasAttribute("checked");
    }
    else if (id == "opt-p-mute")
    {
        menu_.settings.mute = target->HasAttribute("checked");
    }
    else if (id == "opt-p-rightdrag")
    {
        menu_.settings.rightDragPan = target->HasAttribute("checked");
    }
    else if (id == "opt-p-colorblind")
    {
        menu_.settings.colorBlindMode = target->HasAttribute("checked");
        art_.SetColorBlindMode(menu_.settings.colorBlindMode);
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
