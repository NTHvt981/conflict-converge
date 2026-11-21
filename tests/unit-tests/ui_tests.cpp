#include "test_harness.h"

#include "app/ui/RmlUiHost.h"
#include "app/ui/UiIds.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <RmlUi/Core/RenderInterface.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

class HeadlessRenderInterface : public Rml::RenderInterface
{
public:
	Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
												Rml::Span<const int> indices) override
	{
		return ++nextHandle_;
	}

	void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation,
						Rml::TextureHandle texture) override
	{
	}

	void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override {}

	Rml::TextureHandle LoadTexture(Rml::Vector2i &texture_dimensions, const Rml::String &source) override
	{
		return 0;
	}

	Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source,
									   Rml::Vector2i source_dimensions) override
	{
		return ++nextHandle_;
	}

	void ReleaseTexture(Rml::TextureHandle texture) override {}

	void EnableScissorRegion(bool enable) override {}

	void SetScissorRegion(Rml::Rectanglei region) override {}

private:
	Rml::CompiledGeometryHandle nextHandle_ = 0;
};

std::string FindDataRoot()
{
	const std::string candidates[] = { "data", "../data", "../../data" };
	for (const std::string &root : candidates)
	{
		if (std::filesystem::exists(root + "/ui/hud.rml"))
		{
			return root;
		}
	}
	return {};
}

struct LoadedDoc
{
	std::string name;
	Rml::ElementDocument *doc = nullptr;
};

Rml::ElementDocument *FindDoc(const std::vector<LoadedDoc> &docs, const char *name)
{
	for (const LoadedDoc &loaded : docs)
	{
		if (loaded.name == name)
		{
			return loaded.doc;
		}
	}
	return nullptr;
}

bool HasId(Rml::ElementDocument *doc, std::string_view id)
{
	return doc != nullptr && doc->GetElementById(Rml::String(id)) != nullptr;
}

int CountElements(Rml::Element *element)
{
	int count = 1;
	for (int i = 0; i < element->GetNumChildren(); ++i)
	{
		count += CountElements(element->GetChild(i));
	}
	return count;
}

void ExpectIds(Rml::ElementDocument *doc, const char *label,
			   std::initializer_list<std::string_view> ids)
{
	int missing = 0;
	for (std::string_view id : ids)
	{
		if (!HasId(doc, id))
		{
			std::printf("missing id in %s: %s\n", label, std::string(id).c_str());
			++missing;
		}
	}
	CC_CHECK(missing == 0);
}

void ExpectIndexed(Rml::ElementDocument *doc, const char *label, std::string_view prefix,
				   int first, int last)
{
	int missing = 0;
	for (int i = first; i <= last; ++i)
	{
		const std::string id = UiIds::WithIndex(prefix, i);
		if (!HasId(doc, id))
		{
			std::printf("missing indexed id in %s: %s\n", label, id.c_str());
			++missing;
		}
	}
	CC_CHECK(missing == 0);
}

void ExpectBlank(Rml::ElementDocument *doc, std::string_view id)
{
	Rml::Element *element = doc != nullptr ? doc->GetElementById(Rml::String(id)) : nullptr;
	if (element == nullptr)
	{
		std::printf("missing id for blank-text check: %s\n", std::string(id).c_str());
		CC_CHECK(element != nullptr);
		return;
	}
	const Rml::String inner = element->GetInnerRML();
	const bool blank = std::all_of(inner.begin(), inner.end(), [](unsigned char c) {
		return std::isspace(c) != 0;
	});
	if (!blank)
	{
		std::printf("expected blank text in %s, got '%s'\n", std::string(id).c_str(), inner.c_str());
	}
	CC_CHECK(blank);
}

} // namespace

void RunUiTests()
{
	const std::string root = FindDataRoot();
	if (root.empty())
	{
		std::printf("Ui: data root not found (cwd must be the repo root or prj/)\n");
		CC_CHECK(false);
		return;
	}

	HeadlessRenderInterface render;
	RmlUiHost host;
	CC_CHECK(host.Init(root + "/ui", root + "/fonts", &render, 1280, 720));
	CC_CHECK(host.IsReady());
	Rml::Context *context = host.context();
	CC_CHECK(context != nullptr);
	if (context == nullptr)
	{
		return;
	}

	const char *documentNames[] = {
		"hud.rml", "main_menu.rml", "skirmish_setup.rml", "settings.rml", "load_game.rml",
		"confirm.rml", "pause.rml", "outcome.rml", "hotkey_remap.rml",
	};
	std::vector<LoadedDoc> docs;
	for (const char *name : documentNames)
	{
		Rml::ElementDocument *doc = context->LoadDocument(root + "/ui/" + name);
		CC_CHECK(doc != nullptr);
		if (doc != nullptr)
		{
			doc->Show();
			docs.push_back({ name, doc });
		}
	}
	context->Update();
	CC_CHECK(docs.size() == 9);
	if (docs.size() != 9)
	{
		host.Shutdown();
		return;
	}

	for (const LoadedDoc &loaded : docs)
	{
		CC_CHECK(CountElements(loaded.doc) > 5);
		CC_CHECK(loaded.doc->GetClientHeight() > 0.0f);
	}

	Rml::ElementDocument *hud = FindDoc(docs, "hud.rml");
	Rml::ElementDocument *menu = FindDoc(docs, "main_menu.rml");
	Rml::ElementDocument *setup = FindDoc(docs, "skirmish_setup.rml");
	Rml::ElementDocument *settings = FindDoc(docs, "settings.rml");
	Rml::ElementDocument *load = FindDoc(docs, "load_game.rml");
	Rml::ElementDocument *confirm = FindDoc(docs, "confirm.rml");
	Rml::ElementDocument *pause = FindDoc(docs, "pause.rml");
	Rml::ElementDocument *outcome = FindDoc(docs, "outcome.rml");
	Rml::ElementDocument *remap = FindDoc(docs, "hotkey_remap.rml");

	// Every UiIds constant resolves in its owning document.
	ExpectIds(hud, "hud.rml",
			  { UiIds::ResLine, UiIds::SelLine, UiIds::QueueLine, UiIds::FpsLine,
				UiIds::NeedProduction, UiIds::ProdQueueRow, UiIds::HudProducing,
				UiIds::ProducingBootcamp, UiIds::ProducingWorkshop, UiIds::ProducingFillBootcamp,
				UiIds::ProducingFillWorkshop, UiIds::HudHints, UiIds::HintColA, UiIds::HintColB,
				UiIds::BtnCancel, UiIds::TabInfantry, UiIds::TabVehicles, UiIds::TabBuildings,
				UiIds::RowsInfantry, UiIds::RowsVehicles, UiIds::RowsBuildings, UiIds::AbHold,
				UiIds::AbGuard, UiIds::AbPatrol, UiIds::AbAttackMove, UiIds::AbHalt, UiIds::AbRepair,
				UiIds::AbHeal, UiIds::AbRally, UiIds::AbDemolish });
	ExpectIndexed(hud, "hud.rml", UiIds::CgRow, 0, 9);
	ExpectIndexed(hud, "hud.rml", UiIds::ProdInfantryRow, 0, 4);
	ExpectIndexed(hud, "hud.rml", UiIds::RepeatInfantryRow, 0, 4);
	ExpectIndexed(hud, "hud.rml", UiIds::ProdVehicleRow, 0, 3);
	ExpectIndexed(hud, "hud.rml", UiIds::RepeatVehicleRow, 0, 3);
	ExpectIndexed(hud, "hud.rml", UiIds::PlaceRow, 0, 3);

	ExpectIds(menu, "main_menu.rml",
			  { UiIds::BtnSetup, UiIds::BtnLoad, UiIds::BtnSettings, UiIds::BtnEditor,
				UiIds::BtnQuit });

	ExpectIds(setup, "skirmish_setup.rml",
			  { UiIds::MapList, UiIds::MapDetail, UiIds::BtnStart, UiIds::BtnBackRight,
				UiIds::CursorHint });
	ExpectIndexed(setup, "skirmish_setup.rml", UiIds::DifficultyRow, 0, 2);

	ExpectIds(settings, "settings.rml",
			  { UiIds::BtnSettingsBack, UiIds::BtnRemap, UiIds::OptCamSpeed, UiIds::OptMinimap,
				UiIds::OptHints, UiIds::OptMaster, UiIds::OptMusic, UiIds::OptSfx, UiIds::OptMute,
				UiIds::OptRightDrag, UiIds::OptColorBlind, UiIds::OptUiScale, UiIds::ValCamSpeed,
				UiIds::ValMaster, UiIds::ValMusic, UiIds::ValSfx, UiIds::ValUiScale });

	ExpectIds(load, "load_game.rml", { UiIds::BtnLoadBack, UiIds::BtnReplay });
	ExpectIndexed(load, "load_game.rml", UiIds::SlotRow, 0, 3);

	ExpectIds(confirm, "confirm.rml",
			  { UiIds::ConfirmTitle, UiIds::ConfirmSub, UiIds::BtnYes, UiIds::BtnNo });

	ExpectIds(pause, "pause.rml",
			  { UiIds::BtnResume, UiIds::BtnPRemap, UiIds::BtnPToMenu, UiIds::BtnPToDesktop,
				UiIds::OptPCamSpeed, UiIds::OptPMinimap, UiIds::OptPHints, UiIds::OptPMaster,
				UiIds::OptPMusic, UiIds::OptPSfx, UiIds::OptPMute, UiIds::OptPRightDrag,
				UiIds::OptPColorBlind, UiIds::ValPCamSpeed, UiIds::ValPMaster, UiIds::ValPMusic,
				UiIds::ValPSfx });

	ExpectIds(outcome, "outcome.rml",
			  { UiIds::OutcomeTitle, UiIds::OutcomeBody, UiIds::BtnOToMenu, UiIds::BtnOToDesktop });

	ExpectIds(remap, "hotkey_remap.rml",
			  { UiIds::RemapStatus, UiIds::HkColA, UiIds::HkColB, UiIds::BtnRemapBack });

	// Startup text is blank, and no document keeps stale placeholder copy.
	ExpectBlank(hud, UiIds::ResLine);
	ExpectBlank(hud, UiIds::SelLine);
	ExpectBlank(hud, UiIds::QueueLine);
	ExpectBlank(hud, UiIds::FpsLine);
	for (int i = 0; i <= 9; ++i)
	{
		ExpectBlank(hud, UiIds::WithIndex(UiIds::CgRow, i));
	}
	for (std::string_view id :
		 { UiIds::AbHold, UiIds::AbGuard, UiIds::AbPatrol, UiIds::AbAttackMove, UiIds::AbHalt,
		   UiIds::AbRepair, UiIds::AbHeal, UiIds::AbRally, UiIds::AbDemolish })
	{
		ExpectBlank(hud, id);
	}
	for (int i = 0; i <= 4; ++i)
	{
		ExpectBlank(hud, UiIds::WithIndex(UiIds::ProdInfantryRow, i));
		ExpectBlank(hud, UiIds::WithIndex(UiIds::RepeatInfantryRow, i));
	}
	for (int i = 0; i <= 3; ++i)
	{
		ExpectBlank(hud, UiIds::WithIndex(UiIds::ProdVehicleRow, i));
		ExpectBlank(hud, UiIds::WithIndex(UiIds::RepeatVehicleRow, i));
		ExpectBlank(hud, UiIds::WithIndex(UiIds::PlaceRow, i));
	}

	for (const LoadedDoc &loaded : docs)
	{
		const Rml::String markup = loaded.doc->GetInnerRML();
		const bool clean = markup.find("Raze") == Rml::String::npos &&
						   markup.find("Iron: 500") == Rml::String::npos &&
						   markup.find("60 FPS") == Rml::String::npos &&
						   markup.find("Queue: 2") == Rml::String::npos &&
						   markup.find("hint-line") == Rml::String::npos;
		if (!clean)
		{
			std::printf("stale placeholder text in %s\n", loaded.name.c_str());
		}
		CC_CHECK(clean);
	}

	Rml::Element *needProduction = hud->GetElementById(Rml::String(UiIds::NeedProduction));
	CC_CHECK(needProduction != nullptr &&
			 needProduction->GetInnerRML().find("Need Bootcamp / Workshop") != Rml::String::npos);

	// Write paths used by the refresh code.
	Rml::Element *resLine = hud->GetElementById(Rml::String(UiIds::ResLine));
	CC_CHECK(resLine != nullptr);
	if (resLine != nullptr)
	{
		resLine->SetInnerRML("Iron: 500");
		context->Update();
		CC_CHECK(resLine->GetInnerRML() == "Iron: 500");
	}

	Rml::Element *cancel = hud->GetElementById(Rml::String(UiIds::BtnCancel));
	CC_CHECK(cancel != nullptr);
	if (cancel != nullptr)
	{
		cancel->SetAttribute("disabled", "disabled");
		CC_CHECK(cancel->HasAttribute("disabled"));
		cancel->RemoveAttribute("disabled");
		CC_CHECK(!cancel->HasAttribute("disabled"));
	}

	Rml::Element *hints = settings->GetElementById(Rml::String(UiIds::OptHints));
	CC_CHECK(hints != nullptr);
	if (hints != nullptr)
	{
		CC_CHECK(!hints->HasAttribute("checked"));
		hints->SetAttribute("checked", "checked");
		CC_CHECK(hints->HasAttribute("checked"));
		hints->RemoveAttribute("checked");
		CC_CHECK(!hints->HasAttribute("checked"));
	}

	auto *master = dynamic_cast<Rml::ElementFormControl *>(
		settings->GetElementById(Rml::String(UiIds::OptMaster)));
	CC_CHECK(master != nullptr);
	if (master != nullptr)
	{
		master->SetValue("0.35");
		context->Update();
		CC_CHECK(std::stof(master->GetValue()) == 0.35f);
	}

	// Rows rebuilt through SetInnerRML expose their new ids and drop old ones.
	Rml::Element *infantryRows = hud->GetElementById(Rml::String(UiIds::RowsInfantry));
	CC_CHECK(infantryRows != nullptr);
	if (infantryRows != nullptr)
	{
		infantryRows->SetInnerRML(
			"<div class=\"prod-row\"><button class=\"prod-btn\" id=\"prod-test\">"
			"Rifle Infantry</button></div>");
		context->Update();
		CC_CHECK(HasId(hud, "prod-test"));
		infantryRows->SetInnerRML("");
		context->Update();
		CC_CHECK(!HasId(hud, "prod-test"));
	}

	// Rows the menu builds with CreateElement/AppendChild resolve the same way.
	CC_CHECK(!HasId(setup, "mapitem-0"));
	Rml::Element *mapList = setup->GetElementById(Rml::String(UiIds::MapList));
	CC_CHECK(mapList != nullptr);
	if (mapList != nullptr)
	{
		Rml::ElementPtr item(setup->CreateElement("div"));
		item->SetAttribute("id", "mapitem-0");
		item->SetAttribute("class", "map-item");
		item->SetInnerRML("Twin Falls (24x18)");
		mapList->AppendChild(std::move(item));
		context->Update();
		CC_CHECK(HasId(setup, "mapitem-0"));
	}

	CC_CHECK(!HasId(remap, "hkrow-0") && !HasId(remap, "hkkey-0"));
	Rml::Element *column = remap->GetElementById(Rml::String(UiIds::HkColA));
	CC_CHECK(column != nullptr);
	if (column != nullptr)
	{
		Rml::ElementPtr row(remap->CreateElement("div"));
		row->SetAttribute("id", "hkrow-0");
		row->SetAttribute("class", "hkrow");
		Rml::ElementPtr key(remap->CreateElement("button"));
		key->SetAttribute("id", "hkkey-0");
		key->SetAttribute("class", "hkkey");
		key->SetInnerRML("W");
		row->AppendChild(std::move(key));
		column->AppendChild(std::move(row));
		context->Update();
		CC_CHECK(HasId(remap, "hkrow-0") && HasId(remap, "hkkey-0"));
	}

	host.Shutdown();
	CC_CHECK(!host.IsReady());
}
