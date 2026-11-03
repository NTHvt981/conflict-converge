#include "test_harness.h"

#include "app/ui/RmlUiHost.h"
#include "app/ui/UiIds.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{

std::string ReadHudDocument()
{
	const std::string candidates[] = {
		"data/ui/hud.rml",
		"../../data/ui/hud.rml",
		"../../../data/ui/hud.rml",
	};
	for (const std::string &path : candidates)
	{
		std::ifstream file(path, std::ios::binary);
		if (!file)
			continue;
		std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		if (!contents.empty())
			return contents;
	}
	return {};
}

bool DocumentHasId(const std::string &doc, std::string_view id)
{
	return doc.find(std::string("id=\"") + std::string(id) + "\"") != std::string::npos;
}

bool IsBlank(std::string_view text)
{
	return std::all_of(text.begin(), text.end(), [](unsigned char c) {
		return std::isspace(c) != 0;
	});
}

bool DocumentTextIsEmpty(const std::string &doc, std::string_view id)
{
	const std::size_t idPos = doc.find(std::string("id=\"") + std::string(id) + "\"");
	if (idPos == std::string::npos)
		return false;
	const std::size_t tagEnd = doc.find('>', idPos);
	if (tagEnd == std::string::npos)
		return false;
	const std::size_t close = doc.find('<', tagEnd + 1);
	if (close == std::string::npos)
		return false;
	return IsBlank(std::string_view(doc).substr(tagEnd + 1, close - (tagEnd + 1)));
}

} // namespace

void RunRmlUiHostTests()
{
	CC_CHECK(CcNear(RmlUiHost::AutoDpRatio(1280, 720, 1.0f), 1.0f));
	CC_CHECK(CcNear(RmlUiHost::AutoDpRatio(800, 600, 1.0f), 0.625f));
	CC_CHECK(CcNear(RmlUiHost::AutoDpRatio(1600, 360, 1.0f), 0.5f));
	CC_CHECK(CcNear(RmlUiHost::AutoDpRatio(1920, 1080, 1.0f), 1.5f));
	CC_CHECK(CcNear(RmlUiHost::AutoDpRatio(1280, 720, 2.0f), 2.0f));
	CC_CHECK(CcNear(RmlUiHost::AutoDpRatio(3840, 2160, 2.0f), 3.0f));
	CC_CHECK(CcNear(RmlUiHost::AutoDpRatio(320, 200, 0.75f), 0.5f));

	// --- hud.rml carries every id the refresh path looks up, and no runtime text ---
	// RmlUiHost::Init() returns false with no window, so the unit suite cannot load
	// the document through the host; the test checks the document text statically.
	const std::string doc = ReadHudDocument();
	CC_CHECK(!doc.empty());
	if (!doc.empty())
	{
		CC_CHECK(DocumentHasId(doc, UiIds::ResLine));
		CC_CHECK(DocumentHasId(doc, UiIds::SelLine));
		CC_CHECK(DocumentHasId(doc, UiIds::QueueLine));
		CC_CHECK(DocumentHasId(doc, UiIds::FpsLine));
		CC_CHECK(DocumentHasId(doc, UiIds::NeedProduction));
		CC_CHECK(DocumentHasId(doc, UiIds::ProdQueueRow));
		CC_CHECK(DocumentHasId(doc, UiIds::HudProducing));
		CC_CHECK(DocumentHasId(doc, UiIds::ProducingBootcamp));
		CC_CHECK(DocumentHasId(doc, UiIds::ProducingWorkshop));
		CC_CHECK(DocumentHasId(doc, UiIds::ProducingFillBootcamp));
		CC_CHECK(DocumentHasId(doc, UiIds::ProducingFillWorkshop));
		CC_CHECK(DocumentHasId(doc, UiIds::HudHints));
		CC_CHECK(DocumentHasId(doc, UiIds::BtnCancel));
		CC_CHECK(DocumentHasId(doc, UiIds::TabInfantry));
		CC_CHECK(DocumentHasId(doc, UiIds::TabVehicles));
		CC_CHECK(DocumentHasId(doc, UiIds::TabBuildings));
		CC_CHECK(DocumentHasId(doc, UiIds::RowsInfantry));
		CC_CHECK(DocumentHasId(doc, UiIds::RowsVehicles));
		CC_CHECK(DocumentHasId(doc, UiIds::RowsBuildings));
		CC_CHECK(DocumentHasId(doc, UiIds::AbHold));
		CC_CHECK(DocumentHasId(doc, UiIds::AbGuard));
		CC_CHECK(DocumentHasId(doc, UiIds::AbPatrol));
		CC_CHECK(DocumentHasId(doc, UiIds::AbAttackMove));
		CC_CHECK(DocumentHasId(doc, UiIds::AbHalt));
		CC_CHECK(DocumentHasId(doc, UiIds::AbRepair));
		CC_CHECK(DocumentHasId(doc, UiIds::AbHeal));
		CC_CHECK(DocumentHasId(doc, UiIds::AbRally));
		CC_CHECK(DocumentHasId(doc, UiIds::AbDemolish));

		for (int i = 0; i <= 9; ++i)
			CC_CHECK(DocumentHasId(doc, UiIds::WithIndex(UiIds::CgRow, i)));
		for (int i = 0; i <= 3; ++i)
		{
			CC_CHECK(DocumentHasId(doc, UiIds::WithIndex(UiIds::ProdInfantryRow, i)));
			CC_CHECK(DocumentHasId(doc, UiIds::WithIndex(UiIds::ProdVehicleRow, i)));
			CC_CHECK(DocumentHasId(doc, UiIds::WithIndex(UiIds::RepeatInfantryRow, i)));
			CC_CHECK(DocumentHasId(doc, UiIds::WithIndex(UiIds::RepeatVehicleRow, i)));
			CC_CHECK(DocumentHasId(doc, UiIds::WithIndex(UiIds::PlaceRow, i)));
		}

		CC_CHECK(DocumentTextIsEmpty(doc, UiIds::ResLine));
		CC_CHECK(DocumentTextIsEmpty(doc, UiIds::SelLine));
		CC_CHECK(DocumentTextIsEmpty(doc, UiIds::QueueLine));
		CC_CHECK(DocumentTextIsEmpty(doc, UiIds::FpsLine));
		for (int i = 0; i <= 9; ++i)
			CC_CHECK(DocumentTextIsEmpty(doc, UiIds::WithIndex(UiIds::CgRow, i)));
		CC_CHECK(DocumentTextIsEmpty(doc, UiIds::AbHold));
		CC_CHECK(DocumentTextIsEmpty(doc, UiIds::AbGuard));
		CC_CHECK(DocumentTextIsEmpty(doc, UiIds::AbPatrol));
		CC_CHECK(DocumentTextIsEmpty(doc, UiIds::AbAttackMove));
		CC_CHECK(DocumentTextIsEmpty(doc, UiIds::AbHalt));
		CC_CHECK(DocumentTextIsEmpty(doc, UiIds::AbRepair));
		CC_CHECK(DocumentTextIsEmpty(doc, UiIds::AbHeal));
		CC_CHECK(DocumentTextIsEmpty(doc, UiIds::AbRally));
		CC_CHECK(DocumentTextIsEmpty(doc, UiIds::AbDemolish));
		for (int i = 0; i <= 3; ++i)
		{
			CC_CHECK(DocumentTextIsEmpty(doc, UiIds::WithIndex(UiIds::ProdInfantryRow, i)));
			CC_CHECK(DocumentTextIsEmpty(doc, UiIds::WithIndex(UiIds::RepeatInfantryRow, i)));
			CC_CHECK(DocumentTextIsEmpty(doc, UiIds::WithIndex(UiIds::ProdVehicleRow, i)));
			CC_CHECK(DocumentTextIsEmpty(doc, UiIds::WithIndex(UiIds::RepeatVehicleRow, i)));
			CC_CHECK(DocumentTextIsEmpty(doc, UiIds::WithIndex(UiIds::PlaceRow, i)));
		}
		CC_CHECK(DocumentTextIsEmpty(doc, UiIds::HudHints));

		CC_CHECK(doc.find("class=\"hint-line\"") == std::string::npos);
		CC_CHECK(doc.find("Raze") == std::string::npos);
		CC_CHECK(doc.find("Iron: 500") == std::string::npos);
		CC_CHECK(doc.find("60 FPS") == std::string::npos);
		CC_CHECK(doc.find("Queue: 2") == std::string::npos);

		CC_CHECK(doc.find("Need Bootcamp / Workshop") != std::string::npos);
	}
}
