
#include "app/ui/RmlUiHud.h"

#include <cstdio>
#include <cstring>

#include <RmlUi/Core.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>

#include "economy/Building.h"
#include "app/ui/Hud.h"
#include "units/Unit.h"
#include "units/UnitCommands.h"

namespace {

// Ability buttons in panel order; ids match hud.rml.
const std::pair<const char *, AbilityId> kAbilityButtons[] = {
    { "ab-hold", AbilityId::Hold },         { "ab-guard", AbilityId::Guard },
    { "ab-patrol", AbilityId::Patrol },     { "ab-attackmove", AbilityId::AttackMove },
    { "ab-halt", AbilityId::Halt },         { "ab-repair", AbilityId::Repair },
    { "ab-heal", AbilityId::Heal },         { "ab-rally", AbilityId::Rally },
    { "ab-demolish", AbilityId::Demolish },
};

} // namespace

void RmlUiHud::Announce(EventType type)
{
    Event bare;
    bare.type = type;
    events_.Dispatch(bare);
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
