
#include "app/ui/RmlUiHud.h"

#include <cstdio>

#include "raylib.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>

#include "economy/Building.h"
#include "app/ui/Hud.h"
#include "app/ui/UiIds.h"
#include "core/Team.h"
#include "units/Unit.h"
#include "units/UnitFactory.h"

void RmlUiHud::RefreshPause()
{
	SetRangeIn(pauseDoc_, UiIds::OptPCamSpeed, menu_.settings.cameraSpeed);
	SetRangeIn(pauseDoc_, UiIds::OptPMaster, menu_.settings.masterVolume);
	SetRangeIn(pauseDoc_, UiIds::OptPMusic, menu_.settings.musicVolume);
	SetRangeIn(pauseDoc_, UiIds::OptPSfx, menu_.settings.sfxVolume);
	SetCheckIn(pauseDoc_, UiIds::OptPMinimap, menu_.settings.showMinimap);
	SetCheckIn(pauseDoc_, UiIds::OptPHints, menu_.settings.showHints);
	SetCheckIn(pauseDoc_, UiIds::OptPMute, menu_.settings.mute);
	SetCheckIn(pauseDoc_, UiIds::OptPRightDrag, menu_.settings.rightDragPan);
	SetCheckIn(pauseDoc_, UiIds::OptPColorBlind, menu_.settings.colorBlindMode);
	char text[32];
	snprintf(text, sizeof(text), "%g", static_cast<double>(menu_.settings.cameraSpeed));
	SetTextIn(pauseDoc_, UiIds::ValPCamSpeed, text);
	snprintf(text, sizeof(text), "%g", static_cast<double>(menu_.settings.masterVolume));
	SetTextIn(pauseDoc_, UiIds::ValPMaster, text);
	snprintf(text, sizeof(text), "%g", static_cast<double>(menu_.settings.musicVolume));
	SetTextIn(pauseDoc_, UiIds::ValPMusic, text);
	snprintf(text, sizeof(text), "%g", static_cast<double>(menu_.settings.sfxVolume));
	SetTextIn(pauseDoc_, UiIds::ValPSfx, text);
}

void RmlUiHud::RefreshOutcome()
{
	const bool won = menu_.state == MenuState::Victory;
	SetTextIn(outcomeDoc_, UiIds::OutcomeTitle, won ? "Victory!" : "Defeat");
	SetTextIn(outcomeDoc_, UiIds::OutcomeBody,
			  won ? "Enemy force destroyed." : "Your force was destroyed.");
}

void RmlUiHud::RefreshConfirm()
{
	// Texts match the confirm.rml dialog exactly.
	const bool quitting = menu_.confirm == ConfirmKind::QuitApp;
	SetTextIn(confirmDoc_, UiIds::ConfirmTitle,
			  quitting ? "Quit Conflict Converge?" : "Return to main menu?");
	SetTextIn(confirmDoc_, UiIds::ConfirmSub,
			  quitting ? "Close the game?" : "Abandon the current match?");
}

void RmlUiHud::RefreshHud()
{
	SetTextCached(UiIds::ResLine, FormatResources(resources_));
	SetTextCached(UiIds::SelLine, SelectionLine());

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
		const std::string id = UiIds::WithIndex(UiIds::CgRow, bit);
		snprintf(label, sizeof(label), "%d:%d", (bit + 1) % 10, count);
		SetTextCached(id, label);
		if (Rml::Element *el = ById(hudDoc_, id))
		{
			const bool nonempty = count > 0;
			el->SetClass("nonempty", nonempty);
			el->SetClass("allselected", nonempty && allSelected);
			el->SetClass("autogold", bit == autoBit);
		}
	}

	const bool anyProducer = sim_.HasBootcamp() || sim_.HasWorkshop();
	if (Rml::Element *stub = ById(hudDoc_, UiIds::NeedProduction))
	{
		SetDisplay(stub, anyProducer ? "none" : "block");
	}
	ProductionQueue *active = ActiveQueue();
	if (Rml::Element *queueRow = ById(hudDoc_, UiIds::ProdQueueRow))
	{
		SetDisplay(queueRow, active != nullptr ? "block" : "none");
	}
	RefreshProduction();
	RefreshAbilities();
	snprintf(label, sizeof(label), "Queue: %d", active != nullptr ? static_cast<int>(active->Size()) : 0);
	SetTextCached(UiIds::QueueLine, label);

	if (Rml::Element *producing = ById(hudDoc_, UiIds::HudProducing))
	{
		SetDisplay(producing, (active == nullptr || active->Empty()) ? "none" : "block");
	}
	if (Rml::Element *fill = ById(hudDoc_, UiIds::ProducingFill))
	{
		char width[32];
		snprintf(width, sizeof(width), "%g%%",
				 static_cast<double>(active != nullptr ? active->HeadProgress() : 0.0f) * 100.0);
		fill->SetProperty(Rml::String("width"), Rml::String(width));
	}

	snprintf(label, sizeof(label), "%d FPS", GetFPS());
	SetTextCached(UiIds::FpsLine, label);

	if (Rml::Element *hints = ById(hudDoc_, UiIds::HudHints))
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
	if (Rml::Element *rows = ById(hudDoc_, UiIds::RowsInfantry))
	{
		SetDisplay(rows, hasBootcamp && infantryTab ? "block" : "none");
	}
	if (Rml::Element *rows = ById(hudDoc_, UiIds::RowsVehicles))
	{
		SetDisplay(rows, hasWorkshop && vehiclesTab ? "block" : "none");
	}
	if (Rml::Element *rows = ById(hudDoc_, UiIds::RowsBuildings))
	{
		SetDisplay(rows, buildingsTab ? "block" : "none");
	}
	if (Rml::Element *tab = ById(hudDoc_, UiIds::TabInfantry))
	{
		tab->SetClass("selected", infantryTab);
		SetDisabled(tab, !hasBootcamp);
	}
	if (Rml::Element *tab = ById(hudDoc_, UiIds::TabVehicles))
	{
		tab->SetClass("selected", vehiclesTab);
		SetDisabled(tab, !hasWorkshop);
	}
	if (Rml::Element *tab = ById(hudDoc_, UiIds::TabBuildings))
	{
		tab->SetClass("selected", buildingsTab);
	}
	const std::vector<UnitType> infantry = InfantryMenuOrder();
	for (std::size_t i = 0; i < infantry.size() && i < 4; ++i)
	{
		const UnitCost cost = CostOf(infantry[i]);
		const std::string id = UiIds::WithIndex(UiIds::ProdInfantryRow, static_cast<int>(i));
		snprintf(label, sizeof(label), "%s %ld/%ld", UnitTypeName(infantry[i]), cost.iron,
				 cost.oil);
		SetTextCached(id, label);
		if (Rml::Element *el = ById(hudDoc_, id))
		{
			SetDisabled(el, resources_.iron < cost.iron || resources_.oil < cost.oil);
		}
		SetTextCached(UiIds::WithIndex(UiIds::RepeatInfantryRow, static_cast<int>(i)),
					  bootcampQueue_.RepeatArmed(infantry[i]) ? "R*" : "R");
	}
	const std::vector<UnitType> vehicles = VehicleMenuOrder();
	for (std::size_t i = 0; i < vehicles.size() && i < 4; ++i)
	{
		const UnitCost cost = CostOf(vehicles[i]);
		const std::string id = UiIds::WithIndex(UiIds::ProdVehicleRow, static_cast<int>(i));
		snprintf(label, sizeof(label), "%s %ld/%ld", UnitTypeName(vehicles[i]), cost.iron,
				 cost.oil);
		SetTextCached(id, label);
		if (Rml::Element *el = ById(hudDoc_, id))
		{
			SetDisabled(el, resources_.iron < cost.iron || resources_.oil < cost.oil);
		}
		SetTextCached(UiIds::WithIndex(UiIds::RepeatVehicleRow, static_cast<int>(i)),
					  workshopQueue_.RepeatArmed(vehicles[i]) ? "R*" : "R");
	}
	const std::vector<BuildingType> buildings = BuildingMenuOrder();
	for (std::size_t i = 0; i < buildings.size() && i < 4; ++i)
	{
		const std::string id = UiIds::WithIndex(UiIds::PlaceRow, static_cast<int>(i));
		SetTextCached(id, BuildingTypeName(buildings[i]));
	}
	ProductionQueue *active = ActiveQueue();
	snprintf(label, sizeof(label), "Queue: %d",
			 active != nullptr ? static_cast<int>(active->Size()) : 0);
	SetTextCached(UiIds::QueueLine, label);
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
	return SelectionLineText(registry_);
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
	Rml::Element *list = ById(hudDoc_, UiIds::HudHints);
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
