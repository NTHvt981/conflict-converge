
#include "app/ui/RmlUiHud.h"

#include <cstdio>

#include <RmlUi/Core.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>

#include "economy/Building.h"
#include "app/ui/Hud.h"
#include "app/ui/UiIds.h"
#include "units/Unit.h"
#include "units/UnitCommands.h"

namespace {

// Ability buttons in panel order.
const std::pair<std::string_view, AbilityId> kAbilityButtons[] = {
	{ UiIds::AbHold, AbilityId::Hold },         { UiIds::AbGuard, AbilityId::Guard },
	{ UiIds::AbPatrol, AbilityId::Patrol },     { UiIds::AbAttackMove, AbilityId::AttackMove },
	{ UiIds::AbHalt, AbilityId::Halt },         { UiIds::AbRepair, AbilityId::Repair },
	{ UiIds::AbHeal, AbilityId::Heal },         { UiIds::AbRally, AbilityId::Rally },
	{ UiIds::AbDemolish, AbilityId::Demolish },
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
		Rml::Element *el = ById(hudDoc_, buttonId);
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
		SetDisplay(el, entry != nullptr ? "inline-block" : "none");
		if (entry == nullptr)
		{
			continue;
		}
		SetTextCached(buttonId, AbilityName(ability));
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
	if (id == UiIds::BtnCancel)
	{
		if (ProductionQueue *active = ActiveQueue())
		{
			active->CancelTop(resources_);
		}
		return;
	}
	if (id == UiIds::TabInfantry)
	{
		productionTab_ = ProductionTab::Infantry;
		return;
	}
	if (id == UiIds::TabVehicles)
	{
		productionTab_ = ProductionTab::Vehicles;
		return;
	}
	if (id == UiIds::TabBuildings)
	{
		productionTab_ = ProductionTab::Buildings;
		return;
	}
	if (UiIds::StartsWith(id, UiIds::PlaceRow))
	{
		const int row = std::atoi(id.c_str() + UiIds::PlaceRow.size());
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
	auto enqueueRow = [&](std::string_view prefix, const std::vector<UnitType> &order,
						  ProductionQueue &queue) {
		if (!UiIds::StartsWith(id, prefix))
		{
			return false;
		}
		const int row = std::atoi(id.c_str() + prefix.size());
		if (row >= 0 && row < static_cast<int>(order.size()))
		{
			queue.Enqueue(resources_, order[static_cast<std::size_t>(row)],
						  queue.RepeatArmed(order[static_cast<std::size_t>(row)]));
		}
		return true;
	};
	auto repeatRow = [&](std::string_view prefix, const std::vector<UnitType> &order,
						 ProductionQueue &queue) {
		if (!UiIds::StartsWith(id, prefix))
		{
			return false;
		}
		const int row = std::atoi(id.c_str() + prefix.size());
		if (row >= 0 && row < static_cast<int>(order.size()))
		{
			const UnitType type = order[static_cast<std::size_t>(row)];
			queue.SetRepeatArmed(type, !queue.RepeatArmed(type));
		}
		return true;
	};
	if (enqueueRow(UiIds::ProdInfantryRow, infantry, bootcampQueue_) ||
		enqueueRow(UiIds::ProdVehicleRow, vehicles, workshopQueue_) ||
		repeatRow(UiIds::RepeatInfantryRow, infantry, bootcampQueue_) ||
		repeatRow(UiIds::RepeatVehicleRow, vehicles, workshopQueue_))
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
	if (id == UiIds::BtnResume)
	{
		Announce(EventType::MenuAction);
		menu_.state = MenuState::Playing;
	}
	else if (id == UiIds::BtnPRemap)
	{
		beginRemap_(MenuState::Paused);
	}
	else if (id == UiIds::BtnPToMenu)
	{
		quitToMenu_();
	}
	else if (id == UiIds::BtnPToDesktop)
	{
		menu_.quitRequested = true;
	}
	else if (id == UiIds::BtnOToMenu)
	{
		quitToMenu_();
	}
	else if (id == UiIds::BtnOToDesktop)
	{
		menu_.quitRequested = true;
	}
	else if (id == UiIds::BtnYes)
	{
		pendingChoice_ = ConfirmChoice::Yes;
	}
	else if (id == UiIds::BtnNo)
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
				SellBuilding(registry_, map_, resources_, kPlayerTeam, id);
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
	if (id == UiIds::OptPCamSpeed)
	{
		menu_.settings.cameraSpeed = Clamp(ReadRange(target), 100.0f, 800.0f);
		char text[32];
		snprintf(text, sizeof(text), "%g", static_cast<double>(menu_.settings.cameraSpeed));
		SetTextIn(pauseDoc_, UiIds::ValPCamSpeed, text);
	}
	else if (id == UiIds::OptPMaster)
	{
		menu_.settings.masterVolume = Clamp(ReadRange(target), 0.0f, 1.0f);
		char text[32];
		snprintf(text, sizeof(text), "%g", static_cast<double>(menu_.settings.masterVolume));
		SetTextIn(pauseDoc_, UiIds::ValPMaster, text);
	}
	else if (id == UiIds::OptPMusic)
	{
		menu_.settings.musicVolume = Clamp(ReadRange(target), 0.0f, 1.0f);
		char text[32];
		snprintf(text, sizeof(text), "%g", static_cast<double>(menu_.settings.musicVolume));
		SetTextIn(pauseDoc_, UiIds::ValPMusic, text);
	}
	else if (id == UiIds::OptPSfx)
	{
		menu_.settings.sfxVolume = Clamp(ReadRange(target), 0.0f, 1.0f);
		char text[32];
		snprintf(text, sizeof(text), "%g", static_cast<double>(menu_.settings.sfxVolume));
		SetTextIn(pauseDoc_, UiIds::ValPSfx, text);
	}
	else if (id == UiIds::OptPMinimap)
	{
		menu_.settings.showMinimap = target->HasAttribute("checked");
	}
	else if (id == UiIds::OptPHints)
	{
		menu_.settings.showHints = target->HasAttribute("checked");
	}
	else if (id == UiIds::OptPMute)
	{
		menu_.settings.mute = target->HasAttribute("checked");
	}
	else if (id == UiIds::OptPRightDrag)
	{
		menu_.settings.rightDragPan = target->HasAttribute("checked");
	}
	else if (id == UiIds::OptPColorBlind)
	{
		menu_.settings.colorBlindMode = target->HasAttribute("checked");
		art_.SetColorBlindMode(menu_.settings.colorBlindMode);
	}
}
