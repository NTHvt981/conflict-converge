
#include "app/ui/RmlUiHud.h"

#include <cstdio>

#include "raylib.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>

#include "economy/Building.h"
#include "app/ui/Hud.h"
#include "app/input/Selection.h"
#include "units/Unit.h"
#include "units/UnitFactory.h"

// Player team for every HUD readout, matching the raygui panels.
constexpr int kPlayerTeam = 0;

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
