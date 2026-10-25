#include "app/ui/RmlUiHud.h"

#include <initializer_list>

#include "raylib.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>

#include "app/ui/RmlUiHost.h"
#include "app/ui/UiIds.h"

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
	for (int row = 0; row < 4; ++row)
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
	listen(pauseDoc_, "click",
		   { UiIds::BtnResume, UiIds::BtnPRemap, UiIds::BtnPToMenu, UiIds::BtnPToDesktop });
	listen(pauseDoc_, "change",
		   { UiIds::OptPCamSpeed, UiIds::OptPMinimap, UiIds::OptPHints, UiIds::OptPMaster,
			 UiIds::OptPMusic, UiIds::OptPSfx, UiIds::OptPMute, UiIds::OptPRightDrag,
			 UiIds::OptPColorBlind });
	listen(outcomeDoc_, "click", { UiIds::BtnOToMenu, UiIds::BtnOToDesktop });
	listen(confirmDoc_, "click", { UiIds::BtnYes, UiIds::BtnNo });
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
	displayCache_.clear();
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
