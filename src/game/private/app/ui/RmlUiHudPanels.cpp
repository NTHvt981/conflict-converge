
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

    int selectedProducer = -1;
    ProductionTab producerTab = ProductionTab::Infantry;
    bool haveProducer = false;
    registry_.Each<Building>([&](Entity id, const Building &building) {
        if (haveProducer || !building.isSelected)
        {
            return;
        }
        const std::optional<ProductionCategory> category = ProducerCategory(building.type);
        if (!category.has_value())
        {
            return;
        }
        selectedProducer = static_cast<int>(id);
        producerTab = *category == ProductionCategory::Infantry ? ProductionTab::Infantry
                                                                : ProductionTab::Vehicles;
        haveProducer = true;
    });
    if (haveProducer)
    {
        if (selectedProducer != lastSelectedProducer_)
        {
            productionTab_ = producerTab;
            lastSelectedProducer_ = selectedProducer;
        }
    }
    else
    {
        lastSelectedProducer_ = -1;
    }

    char label[64];
    const int autoBit = playingInput_.AutoAddGroupBit();
    int counts[10] = {};
    bool partial[10] = {};
    registry_.Each<Unit>([&](Entity, const Unit &unit) {
        if (unit.teamID != kPlayerTeam)
        {
            return;
        }
        for (int bit = 0; bit < 10; ++bit)
        {
            if ((unit.controlGroups & (1u << static_cast<unsigned int>(bit))) != 0)
            {
                ++counts[bit];
                if (!unit.isSelected)
                {
                    partial[bit] = true;
                }
            }
        }
    });
    for (int bit = 0; bit < 10; ++bit)
    {
        const int count = counts[bit];
        const bool allSelected = !partial[bit];
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

    const bool anyProducer = sim_.HasBootcamp() || sim_.HasWorkshop();
    if (Rml::Element *stub = hudDoc_->GetElementById("need-production"))
    {
        SetDisplay(stub, anyProducer ? "none" : "block");
    }
    ProductionQueue *active = ActiveQueue();
    if (Rml::Element *queueRow = hudDoc_->GetElementById("prod-queue-row"))
    {
        SetDisplay(queueRow, active != nullptr ? "block" : "none");
    }
    RefreshProduction();
    RefreshAbilities();
    snprintf(label, sizeof(label), "Queue: %d", active != nullptr ? static_cast<int>(active->Size()) : 0);
    SetTextCached("queue-line", label);

    if (Rml::Element *producing = hudDoc_->GetElementById("hud-producing"))
    {
        SetDisplay(producing, (active == nullptr || active->Empty()) ? "none" : "block");
    }
    if (Rml::Element *fill = hudDoc_->GetElementById("producing-fill"))
    {
        char width[32];
        snprintf(width, sizeof(width), "%g%%",
                 static_cast<double>(active != nullptr ? active->HeadProgress() : 0.0f) * 100.0);
        fill->SetProperty(Rml::String("width"), Rml::String(width));
    }

    snprintf(label, sizeof(label), "%d FPS", GetFPS());
    SetTextCached("fps-line", label);

    if (Rml::Element *hints = hudDoc_->GetElementById("hud-hints"))
    {
        SetDisplay(hints, showHints_ ? "block" : "none");
    }
    RefreshHints();
}

void RmlUiHud::RefreshProduction()
{
    char label[64];
    const bool hasBootcamp = sim_.HasBootcamp();
    const bool hasWorkshop = sim_.HasWorkshop();
    const bool infantryTab = productionTab_ == ProductionTab::Infantry;
    const bool vehiclesTab = productionTab_ == ProductionTab::Vehicles;
    const bool buildingsTab = productionTab_ == ProductionTab::Buildings;
    if (Rml::Element *rows = hudDoc_->GetElementById("rows-infantry"))
    {
        SetDisplay(rows, hasBootcamp && infantryTab ? "block" : "none");
    }
    if (Rml::Element *rows = hudDoc_->GetElementById("rows-vehicles"))
    {
        SetDisplay(rows, hasWorkshop && vehiclesTab ? "block" : "none");
    }
    if (Rml::Element *rows = hudDoc_->GetElementById("rows-buildings"))
    {
        SetDisplay(rows, buildingsTab ? "block" : "none");
    }
    if (Rml::Element *tab = hudDoc_->GetElementById("tab-infantry"))
    {
        tab->SetClass("selected", infantryTab);
        SetDisabled(tab, !hasBootcamp);
    }
    if (Rml::Element *tab = hudDoc_->GetElementById("tab-vehicles"))
    {
        tab->SetClass("selected", vehiclesTab);
        SetDisabled(tab, !hasWorkshop);
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
        snprintf(id, sizeof(id), "prod-i-%d", static_cast<int>(i));
        snprintf(label, sizeof(label), "%s %ld/%ld", UnitTypeName(infantry[i]), cost.iron,
                 cost.oil);
        SetTextCached(id, label);
        if (Rml::Element *el = hudDoc_->GetElementById(id))
        {
            SetDisabled(el, resources_.iron < cost.iron || resources_.oil < cost.oil);
        }
        snprintf(id, sizeof(id), "repr-i-%d", static_cast<int>(i));
        SetTextCached(id, bootcampQueue_.RepeatArmed(infantry[i]) ? "R*" : "R");
    }
    const std::vector<UnitType> vehicles = VehicleMenuOrder();
    for (std::size_t i = 0; i < vehicles.size() && i < 4; ++i)
    {
        const UnitCost cost = CostOf(vehicles[i]);
        char id[16];
        snprintf(id, sizeof(id), "prod-v-%d", static_cast<int>(i));
        snprintf(label, sizeof(label), "%s %ld/%ld", UnitTypeName(vehicles[i]), cost.iron,
                 cost.oil);
        SetTextCached(id, label);
        if (Rml::Element *el = hudDoc_->GetElementById(id))
        {
            SetDisabled(el, resources_.iron < cost.iron || resources_.oil < cost.oil);
        }
        snprintf(id, sizeof(id), "repr-v-%d", static_cast<int>(i));
        SetTextCached(id, workshopQueue_.RepeatArmed(vehicles[i]) ? "R*" : "R");
    }
    const std::vector<BuildingType> buildings = BuildingMenuOrder();
    for (std::size_t i = 0; i < buildings.size() && i < 4; ++i)
    {
        char id[16];
        snprintf(id, sizeof(id), "place-%d", static_cast<int>(i));
        SetTextCached(id, BuildingTypeName(buildings[i]));
    }
    ProductionQueue *active = ActiveQueue();
    snprintf(label, sizeof(label), "Queue: %d",
             active != nullptr ? static_cast<int>(active->Size()) : 0);
    SetTextCached("queue-line", label);
}

ProductionQueue *RmlUiHud::ActiveQueue()
{
    switch (productionTab_)
    {
    case ProductionTab::Infantry:
        return &bootcampQueue_;
    case ProductionTab::Vehicles:
        return &workshopQueue_;
    case ProductionTab::Buildings:
        return nullptr;
    }
    return nullptr;
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
