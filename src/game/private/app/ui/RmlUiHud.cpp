#include "app/ui/RmlUiHud.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <ranges>
#include <utility>

#include "raylib.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>

#include "app/ui/Hud.h"
#include "app/ui/RmlUiHost.h"
#include "app/ui/UiIds.h"
#include "app/ui/UiWidget.h"
#include "core/Team.h"
#include "economy/Building.h"
#include "units/Unit.h"
#include "units/UnitCommands.h"
#include "units/UnitFactory.h"

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

// Ability buttons in panel order.
const std::pair<std::string_view, AbilityId> kAbilityButtons[] = {
	{ UiIds::AbHold, AbilityId::Hold },         { UiIds::AbGuard, AbilityId::Guard },
	{ UiIds::AbPatrol, AbilityId::Patrol },     { UiIds::AbAttackMove, AbilityId::AttackMove },
	{ UiIds::AbHalt, AbilityId::Halt },         { UiIds::AbRepair, AbilityId::Repair },
	{ UiIds::AbHeal, AbilityId::Heal },         { UiIds::AbRally, AbilityId::Rally },
	{ UiIds::AbDemolish, AbilityId::Demolish },
};

} // namespace

RmlUiHud::RmlUiHud(Registry &registry, ResourceSystem &resources,
				   ProductionQueue &bootcampQueue, ProductionQueue &workshopQueue,
				   Simulation &sim, HotkeyMap &hotkeys, PlayingInput &playingInput,
				   TileMap &map, OccupancyGrid &occ, const bool &showHints,
				   MenuFlow &menu, Art &art, EventDispatcher &events,
				   std::function<void()> quitToMenu,
				   std::function<void(MenuState)> beginRemap)
	: registry_(registry)
	, resources_(resources)
	, bootcampQueue_(bootcampQueue)
	, workshopQueue_(workshopQueue)
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

RmlUiHud::~RmlUiHud() = default;

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
	if (Rml::Element *el = ById(hudDoc_, UiIds::BtnCancel))
	{
		el->AddEventListener("click", this);
	}
	auto listen = [this](Rml::ElementDocument *doc, const char *event,
						 std::initializer_list<std::string_view> ids) {
		for (std::string_view id : ids)
		{
			if (Rml::Element *el = ById(doc, id))
			{
				el->AddEventListener(event, this);
			}
		}
	};
	listen(hudDoc_, "click",
		   { UiIds::TabInfantry, UiIds::TabVehicles, UiIds::TabBuildings, UiIds::AbHold,
			 UiIds::AbGuard, UiIds::AbPatrol, UiIds::AbAttackMove, UiIds::AbHalt, UiIds::AbRepair,
			 UiIds::AbHeal, UiIds::AbRally, UiIds::AbDemolish });
	for (int row = 0; row < 5; ++row)
	{
		for (std::string_view family :
			 { UiIds::PlaceRow, UiIds::ProdInfantryRow, UiIds::ProdVehicleRow,
			   UiIds::RepeatInfantryRow, UiIds::RepeatVehicleRow })
		{
			if (Rml::Element *el = ById(hudDoc_, UiIds::WithIndex(family, row)))
			{
				el->AddEventListener("click", this);
			}
		}
	}
	for (Rml::ElementDocument *doc : { pauseDoc_, outcomeDoc_ })
	{
		(void)doc;
	}
	pauseDoc_->AddEventListener("click", this);
	pauseDoc_->AddEventListener("change", this);
	listen(outcomeDoc_, "click", { UiIds::BtnOToMenu, UiIds::BtnOToDesktop });
	listen(confirmDoc_, "click", { UiIds::BtnYes, UiIds::BtnNo });
	BuildPauseWidgets();
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

void RmlUiHud::ReloadStyleSheets()
{
	if (!ready_)
	{
		return;
	}
	for (Rml::ElementDocument *doc : { hudDoc_, pauseDoc_, outcomeDoc_, confirmDoc_ })
	{
		if (doc != nullptr)
		{
			doc->ReloadStyleSheet();
		}
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
	pauseWidgets_.reset();
	displayCache_.clear();
	hintsCache_.clear();
}

void RmlUiHud::UpdatePointer()
{
	if (!ready_ || context_ == nullptr || menu_.state == MenuState::HotkeyRemap)
	{
		return;
	}
	PumpMouse(context_);
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

void RmlUiHud::BuildPauseWidgets()
{
	pauseWidgets_ = std::make_unique<UiWidget>("pause");
	pauseWidgets_->Add<UiButton>(UiIds::BtnResume)->SetHandler([this] {
		Announce(EventType::MenuAction);
		menu_.state = MenuState::Playing;
	});
	pauseWidgets_->Add<UiButton>(UiIds::BtnPRemap)->SetHandler([this] {
		beginRemap_(MenuState::Paused);
	});
	pauseWidgets_->Add<UiButton>(UiIds::BtnPToMenu)->SetHandler(quitToMenu_);
	pauseWidgets_->Add<UiButton>(UiIds::BtnPToDesktop)->SetHandler([this] {
		menu_.quitRequested = true;
	});

	pauseWidgets_->Add<UiSlider>(
		UiIds::OptPCamSpeed, [this](float value) { menu_.settings.cameraSpeed = value; }, 100.0f,
		800.0f);
	pauseWidgets_->Add<UiSlider>(
		UiIds::OptPMaster, [this](float value) { menu_.settings.masterVolume = value; }, 0.0f, 1.0f);
	pauseWidgets_->Add<UiSlider>(
		UiIds::OptPMusic, [this](float value) { menu_.settings.musicVolume = value; }, 0.0f, 1.0f);
	pauseWidgets_->Add<UiSlider>(
		UiIds::OptPSfx, [this](float value) { menu_.settings.sfxVolume = value; }, 0.0f, 1.0f);

	pauseWidgets_->Add<UiToggle>(UiIds::OptPMinimap, [this](bool value) {
		menu_.settings.showMinimap = value;
	});
	pauseWidgets_->Add<UiToggle>(UiIds::OptPHints, [this](bool value) {
		menu_.settings.showHints = value;
	});
	pauseWidgets_->Add<UiToggle>(UiIds::OptPMute, [this](bool value) {
		menu_.settings.mute = value;
	});
	pauseWidgets_->Add<UiToggle>(UiIds::OptPRightDrag, [this](bool value) {
		menu_.settings.rightDragPan = value;
	});
	pauseWidgets_->Add<UiToggle>(UiIds::OptPColorBlind, [this](bool value) {
		menu_.settings.colorBlindMode = value;
		art_.SetColorBlindMode(value);
	});
}

UiEvent RmlUiHud::Translate(const Rml::Event &event, Rml::Element *target)
{
	UiEvent out;
	if (event.GetType() != "change")
	{
		return out;
	}

	auto *control = dynamic_cast<Rml::ElementFormControl *>(target);
	if (control != nullptr && control->GetAttribute("type", Rml::String()) == "range")
	{
		out.kind = UiEventKind::Value;
		out.value = ReadRange(target);
		return out;
	}

	out.kind = UiEventKind::Toggle;
	out.checked = target->HasAttribute("checked");
	return out;
}

bool RmlUiHud::DispatchWidget(Rml::Event &event, Rml::Element *target)
{
	if (pauseWidgets_ == nullptr)
	{
		return false;
	}

	UiWidget *widget = pauseWidgets_->Find(target->GetId());
	if (widget == nullptr || !widget->HandleEvent(Translate(event, target)))
	{
		return false;
	}

	event.StopPropagation();
	return true;
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
		if (!DispatchWidget(event, target))
		{
			OnClick(id);
		}
	}
	else if (type == "change")
	{
		DispatchWidget(event, target);
	}
}

void RmlUiHud::SetDisabled(Rml::Element *el, bool disabled)
{
	el->SetClass("disabled", disabled);
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

void RmlUiHud::SetTextCached(std::string_view id, const std::string &text)
{
	const std::string key(id);
	auto it = textCache_.find(key);
	if (it != textCache_.end() && it->second == text)
	{
		return;
	}
	textCache_[key] = text;
	if (Rml::Element *el = ById(hudDoc_, id))
	{
		el->SetInnerRML(text.c_str());
	}
}

void RmlUiHud::SetDisplay(Rml::Element *el, const std::string &value)
{
	if (el == nullptr)
	{
		return;
	}
	auto it = displayCache_.find(el);
	if (it != displayCache_.end() && it->second == value)
	{
		return;
	}
	displayCache_[el] = value;
	el->SetProperty("display", value.c_str());
}

Rml::Element *RmlUiHud::ById(Rml::ElementDocument *doc, std::string_view id)
{
	if (doc == nullptr)
	{
		return nullptr;
	}
	return doc->GetElementById(std::string(id));
}

void RmlUiHud::SetRangeIn(Rml::ElementDocument *doc, std::string_view id, float value)
{
	if (Rml::Element *el = ById(doc, id))
	{
		if (auto *control = dynamic_cast<Rml::ElementFormControl *>(el))
		{
			char text[32];
			snprintf(text, sizeof(text), "%g", static_cast<double>(value));
			control->SetValue(text);
		}
	}
}

void RmlUiHud::SetCheckIn(Rml::ElementDocument *doc, std::string_view id, bool checked)
{
	if (Rml::Element *el = ById(doc, id))
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

void RmlUiHud::SetTextIn(Rml::ElementDocument *doc, std::string_view id, const std::string &text)
{
	if (Rml::Element *el = ById(doc, id))
	{
		el->SetInnerRML(text.c_str());
	}
}

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
	const bool anyQueued = !bootcampQueue_.Empty() || !workshopQueue_.Empty();
	if (Rml::Element *queueRow = ById(hudDoc_, UiIds::ProdQueueRow))
	{
		SetDisplay(queueRow, (active != nullptr || anyQueued) ? "block" : "none");
	}
	if (Rml::Element *cancel = ById(hudDoc_, UiIds::BtnCancel))
	{
		SetDisplay(cancel, active != nullptr ? "inline-block" : "none");
	}
	RefreshProduction();
	RefreshAbilities();
	SetTextCached(UiIds::QueueLine,
				  QueueLineText(static_cast<int>(bootcampQueue_.Size()),
								static_cast<int>(workshopQueue_.Size())));

	if (Rml::Element *producing = ById(hudDoc_, UiIds::HudProducing))
	{
		SetDisplay(producing, anyQueued ? "block" : "none");
	}
	RefreshProducingRow(UiIds::ProducingBootcamp, UiIds::ProducingFillBootcamp, bootcampQueue_);
	RefreshProducingRow(UiIds::ProducingWorkshop, UiIds::ProducingFillWorkshop, workshopQueue_);

	snprintf(label, sizeof(label), "%d FPS", GetFPS());
	SetTextCached(UiIds::FpsLine, label);

	if (Rml::Element *hints = ById(hudDoc_, UiIds::HudHints))
	{
		SetDisplay(hints, showHints_ ? "flex" : "none");
	}
	RefreshHints();
}

void RmlUiHud::RefreshProduction()
{
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
	for (const auto &[i, type] : infantry | std::views::take(5) | std::views::enumerate)
	{
		const UnitCost cost = CostOf(type);
		const std::string id = UiIds::WithIndex(UiIds::ProdInfantryRow, static_cast<int>(i));
		SetTextCached(id, ProductionRowText(UnitTypeName(type), cost));
		if (Rml::Element *el = ById(hudDoc_, id))
		{
			SetDisabled(el, !CanAfford(resources_, cost));
		}
		SetTextCached(UiIds::WithIndex(UiIds::RepeatInfantryRow, static_cast<int>(i)),
					  bootcampQueue_.RepeatArmed(type) ? "R*" : "R");
	}
	const std::vector<UnitType> vehicles = VehicleMenuOrder();
	for (const auto &[i, type] : vehicles | std::views::take(4) | std::views::enumerate)
	{
		const UnitCost cost = CostOf(type);
		const std::string id = UiIds::WithIndex(UiIds::ProdVehicleRow, static_cast<int>(i));
		SetTextCached(id, ProductionRowText(UnitTypeName(type), cost));
		if (Rml::Element *el = ById(hudDoc_, id))
		{
			SetDisabled(el, !CanAfford(resources_, cost));
		}
		SetTextCached(UiIds::WithIndex(UiIds::RepeatVehicleRow, static_cast<int>(i)),
					  workshopQueue_.RepeatArmed(type) ? "R*" : "R");
	}
	const std::vector<BuildingType> buildings = BuildingMenuOrder();
	for (const auto &[i, type] : buildings | std::views::take(4) | std::views::enumerate)
	{
		const UnitCost cost = BuildingCost(type);
		const std::string id = UiIds::WithIndex(UiIds::PlaceRow, static_cast<int>(i));
		SetTextCached(id, ProductionRowText(BuildingTypeName(type), cost));
		if (Rml::Element *el = ById(hudDoc_, id))
		{
			SetDisabled(el, !CanAfford(resources_, cost));
		}
	}
}

void RmlUiHud::RefreshProducingRow(std::string_view rowId, std::string_view fillId,
								   const ProductionQueue &queue)
{
	const bool busy = !queue.Empty();
	if (Rml::Element *row = ById(hudDoc_, rowId))
	{
		SetDisplay(row, busy ? "block" : "none");
	}
	if (Rml::Element *fill = ById(hudDoc_, fillId))
	{
		char width[32];
		snprintf(width, sizeof(width), "%g%%",
				 static_cast<double>(queue.HeadProgress()) * 100.0);
		fill->SetProperty(Rml::String("width"), Rml::String(width));
	}
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
	Rml::Element *columns[] = {ById(hudDoc_, UiIds::HintColA), ById(hudDoc_, UiIds::HintColB)};
	if (columns[0] == nullptr || columns[1] == nullptr)
	{
		return;
	}
	for (Rml::Element *column : columns)
	{
		while (Rml::Element *child = column->GetFirstChild())
		{
			column->RemoveChild(child);
		}
	}
	const std::ptrdiff_t split = static_cast<std::ptrdiff_t>((hints.size() + 1) / 2);
	for (const auto &[i, hint] : hints | std::views::enumerate)
	{
		Rml::ElementPtr item(hudDoc_->CreateElement("p"));
		item->SetAttribute("class", Rml::String("hint-line"));
		item->SetInnerRML(hint.c_str());
		columns[i < split ? 0 : 1]->AppendChild(std::move(item));
	}
}

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
	if (id == UiIds::BtnOToMenu)
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
