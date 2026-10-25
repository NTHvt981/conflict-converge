
#include "app/ui/RmlUiMenus.h"

#include <cstdio>
#include <cstring>
#include <filesystem>

#include "raygui.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>

#include "app/data/Art.h"
#include "app/input/Hotkeys.h"
#include "app/ui/UiIds.h"
#include "world/MapFile.h"
#include "app/match/Menu.h"
#include "app/save/SaveGame.h"

void RmlUiMenus::OnClick(const Rml::String &id)
{
	if (id == UiIds::BtnSetup)
	{
		menu_.OpenSetup(ListMaps("data/maps"));
		Announce(EventType::MenuAction);
	}
	else if (id == UiIds::BtnLoad)
	{
		menu_.OpenLoad();
		Announce(EventType::MenuAction);
	}
	else if (id == UiIds::BtnSettings)
	{
		menu_.OpenSettings();
		Announce(EventType::MenuAction);
	}
	else if (id == UiIds::BtnEditor)
	{
		screens_.OpenEditor();
		Announce(EventType::MenuAction);
	}
	else if (id == UiIds::BtnQuit)
	{
		menu_.OpenQuitConfirm();
		Announce(EventType::MenuAction);
	}
	else if (id == UiIds::BtnStart)
	{
		// No announce here, matching the raygui Start path.
		if (const MapEntry *sel = menu_.setup.SelectedMap();
			sel != nullptr && menu_.StartMatch())
		{
			callbacks_.startMatch(sel->path, menu_.setup.difficulty);
		}
	}
	else if (id == UiIds::BtnBackRight)
	{
		menu_.OpenMainMenu();
		Announce(EventType::MenuAction);
	}
	else if (id == UiIds::BtnSettingsBack)
	{
		menu_.settings.hotkeyOverrides = hotkeys_.Overrides();
		SaveSettings(menu_.settings, kSettingsPath);
		menu_.OpenMainMenu();
		Announce(EventType::MenuAction);
	}
	else if (id == UiIds::BtnLoadBack)
	{
		menu_.OpenMainMenu();
		Announce(EventType::MenuAction);
	}
	else if (id == UiIds::BtnRemap)
	{
		BeginRemap(MenuState::Settings);
	}
	else if (id == UiIds::BtnRemapBack)
	{
		remapArming_ = -1;
		remapConflictAction_.clear();
		Announce(EventType::MenuAction);
		menu_.state = remapReturn_;
	}
	else if (UiIds::StartsWith(id, UiIds::RemapKeyRow))
	{
		// Mirror the raygui row button: a second click on the conflict row
		// steals the key, otherwise the click toggles arming.
		const int row = std::atoi(id.c_str() + UiIds::RemapKeyRow.size());
		if (row >= 0 && row < NumHotkeyDefs())
		{
			const std::string action = kHotkeyDefs[row].action;
			if (!remapConflictAction_.empty() && remapConflictAction_ == action)
			{
				hotkeys_.Rebind(action, remapConflictKey_);
				callbacks_.hotkeysRebound();
				menu_.settings.hotkeyOverrides = hotkeys_.Overrides();
				SaveSettings(menu_.settings, kSettingsPath);
				remapConflictAction_.clear();
				remapArming_ = -1;
			}
			else
			{
				remapArming_ = (remapArming_ == row) ? -1 : row;
				remapConflictAction_.clear();
			}
		}
	}
	else if (id == UiIds::BtnYes)
	{
		pendingChoice_ = ConfirmChoice::Yes;
	}
	else if (id == UiIds::BtnNo)
	{
		pendingChoice_ = ConfirmChoice::No;
	}
	else if (UiIds::StartsWith(id, UiIds::DifficultyRow))
	{
		const int row = std::atoi(id.c_str() + UiIds::DifficultyRow.size());
		if (row >= 0 && row <= static_cast<int>(AIDifficulty::Hard))
		{
			menu_.SelectDifficulty(static_cast<AIDifficulty>(row));
		}
	}
	else if (UiIds::StartsWith(id, UiIds::MapRow))
	{
		menu_.SelectMap(std::atoi(id.c_str() + UiIds::MapRow.size()));
	}
	else if (UiIds::StartsWith(id, UiIds::SlotRow))
	{
		// No announce here, matching the raygui slot path. The disabled
		// class is visual-only, so re-check filled before loading.
		const int slot = std::atoi(id.c_str() + UiIds::SlotRow.size());
		if (slot >= 0 && slot < 4)
		{
			const std::string path = slot == 0 ? "data/quicksave.ccpb" : SaveSlotPath(slot);
			std::error_code ec;
			if (std::filesystem::exists(path, ec) && !ec)
			{
				callbacks_.loadSlot(path);
			}
		}
	}
	else if (id == UiIds::BtnReplay)
	{
		if (ReplayFrameCount(kReplayDir) > 0)
		{
			callbacks_.watchReplay();
		}
	}
	else
	{
		return;
	}
	ShowState();
}

void RmlUiMenus::OnChange(Rml::Element *target, const Rml::String &id)
{
	// NOTE: never call RefreshSettings() from here. Programmatic
	// SetValue/SetAttribute calls (e.g. checkbox "checked") dispatch
	// Change back into this listener, so a refresh would recurse until
	// the stack overflows. Update settings + the value label only; the
	// control already shows the new value.
	char text[32];
	if (id == UiIds::OptCamSpeed)
	{
		menu_.settings.cameraSpeed = Clamp(ReadRange(target), 100.0f, 800.0f);
		snprintf(text, sizeof(text), "%g", static_cast<double>(menu_.settings.cameraSpeed));
		SetTextIn(settingsDoc_, UiIds::ValCamSpeed, text);
	}
	else if (id == UiIds::OptMaster)
	{
		menu_.settings.masterVolume = Clamp(ReadRange(target), 0.0f, 1.0f);
		snprintf(text, sizeof(text), "%g", static_cast<double>(menu_.settings.masterVolume));
		SetTextIn(settingsDoc_, UiIds::ValMaster, text);
	}
	else if (id == UiIds::OptMusic)
	{
		menu_.settings.musicVolume = Clamp(ReadRange(target), 0.0f, 1.0f);
		snprintf(text, sizeof(text), "%g", static_cast<double>(menu_.settings.musicVolume));
		SetTextIn(settingsDoc_, UiIds::ValMusic, text);
	}
	else if (id == UiIds::OptSfx)
	{
		menu_.settings.sfxVolume = Clamp(ReadRange(target), 0.0f, 1.0f);
		snprintf(text, sizeof(text), "%g", static_cast<double>(menu_.settings.sfxVolume));
		SetTextIn(settingsDoc_, UiIds::ValSfx, text);
	}
	else if (id == UiIds::OptUiScale)
	{
		menu_.settings.uiScale = Clamp(ReadRange(target), 0.75f, 2.0f);
		// Keeps the raygui Map Editor legible; the RmlUi ratio follows
		// uiScale every frame via BeginFrame.
		GuiSetStyle(DEFAULT, TEXT_SIZE,
					static_cast<int>(10 * menu_.settings.uiScale));
		snprintf(text, sizeof(text), "%g", static_cast<double>(menu_.settings.uiScale));
		SetTextIn(settingsDoc_, UiIds::ValUiScale, text);
	}
	else if (id == UiIds::OptMinimap)
	{
		menu_.settings.showMinimap = target->HasAttribute("checked");
	}
	else if (id == UiIds::OptHints)
	{
		menu_.settings.showHints = target->HasAttribute("checked");
	}
	else if (id == UiIds::OptMute)
	{
		menu_.settings.mute = target->HasAttribute("checked");
	}
	else if (id == UiIds::OptRightDrag)
	{
		menu_.settings.rightDragPan = target->HasAttribute("checked");
	}
	else if (id == UiIds::OptColorBlind)
	{
		menu_.settings.colorBlindMode = target->HasAttribute("checked");
		art_.SetColorBlindMode(menu_.settings.colorBlindMode);
	}
	else
	{
		return;
	}
}

void RmlUiMenus::PopulateMaps()
{
	std::string signature;
	for (const MapEntry &entry : menu_.setup.maps)
	{
		signature += entry.path;
		signature += '\n';
	}
	if (signature == mapCache_)
	{
		return;
	}
	mapCache_ = signature;
	Rml::Element *list = ById(setupDoc_, UiIds::MapList);
	while (Rml::Element *child = list->GetFirstChild())
	{
		list->RemoveChild(child);
	}
	const std::vector<MapEntry> &maps = menu_.setup.maps;
	for (std::size_t i = 0; i < maps.size(); ++i)
	{
		const MapEntry &entry = maps[i];
		Rml::ElementPtr item(setupDoc_->CreateElement("div"));
		const std::string id = UiIds::WithIndex(UiIds::MapRow, static_cast<int>(i));
		item->SetAttribute("id", Rml::String(id));
		item->SetAttribute("class", Rml::String("map-item"));
		char label[128];
		snprintf(label, sizeof(label), "%s (%dx%d)", entry.name.c_str(), entry.width,
				 entry.height);
		item->SetInnerRML(label);
		item->AddEventListener("click", this);
		list->AppendChild(std::move(item));
	}
}

void RmlUiMenus::RefreshSetup()
{
	PopulateMaps();
	for (std::size_t i = 0; i < menu_.setup.maps.size(); ++i)
	{
		const std::string id = UiIds::WithIndex(UiIds::MapRow, static_cast<int>(i));
		if (Rml::Element *el = ById(setupDoc_, id))
		{
			el->SetClass("selected", static_cast<int>(i) == menu_.setup.mapIndex);
		}
	}
	if (Rml::Element *detail = ById(setupDoc_, UiIds::MapDetail))
	{
		if (const MapEntry *sel = menu_.setup.SelectedMap())
		{
			char text[256];
			snprintf(text, sizeof(text), "by %s  %s", sel->author.empty() ? "-" : sel->author.c_str(),
					 sel->path.c_str());
			detail->SetInnerRML(text);
		}
		else
		{
			detail->SetInnerRML("");
		}
	}
	const int diff = static_cast<int>(menu_.setup.difficulty);
	for (int i = 0; i <= static_cast<int>(AIDifficulty::Hard); ++i)
	{
		const std::string id = UiIds::WithIndex(UiIds::DifficultyRow, i);
		if (Rml::Element *el = ById(setupDoc_, id))
		{
			el->SetClass("selected", i == diff);
		}
	}
	// The disabled class is visual-only; the click guard lives in OnClick.
	if (Rml::Element *start = ById(setupDoc_, UiIds::BtnStart))
	{
		SetDisabled(start, !menu_.setup.CanStart());
	}
}

void RmlUiMenus::RefreshSettings()
{
	SetRangeIn(settingsDoc_, UiIds::OptCamSpeed, menu_.settings.cameraSpeed);
	SetRangeIn(settingsDoc_, UiIds::OptMaster, menu_.settings.masterVolume);
	SetRangeIn(settingsDoc_, UiIds::OptMusic, menu_.settings.musicVolume);
	SetRangeIn(settingsDoc_, UiIds::OptSfx, menu_.settings.sfxVolume);
	SetRangeIn(settingsDoc_, UiIds::OptUiScale, menu_.settings.uiScale);
	SetCheckIn(settingsDoc_, UiIds::OptMinimap, menu_.settings.showMinimap);
	SetCheckIn(settingsDoc_, UiIds::OptHints, menu_.settings.showHints);
	SetCheckIn(settingsDoc_, UiIds::OptMute, menu_.settings.mute);
	SetCheckIn(settingsDoc_, UiIds::OptRightDrag, menu_.settings.rightDragPan);
	SetCheckIn(settingsDoc_, UiIds::OptColorBlind, menu_.settings.colorBlindMode);
	char text[32];
	snprintf(text, sizeof(text), "%g", static_cast<double>(menu_.settings.cameraSpeed));
	SetTextIn(settingsDoc_, UiIds::ValCamSpeed, text);
	snprintf(text, sizeof(text), "%g", static_cast<double>(menu_.settings.masterVolume));
	SetTextIn(settingsDoc_, UiIds::ValMaster, text);
	snprintf(text, sizeof(text), "%g", static_cast<double>(menu_.settings.musicVolume));
	SetTextIn(settingsDoc_, UiIds::ValMusic, text);
	snprintf(text, sizeof(text), "%g", static_cast<double>(menu_.settings.sfxVolume));
	SetTextIn(settingsDoc_, UiIds::ValSfx, text);
	snprintf(text, sizeof(text), "%g", static_cast<double>(menu_.settings.uiScale));
	SetTextIn(settingsDoc_, UiIds::ValUiScale, text);
}

void RmlUiMenus::RefreshLoad()
{
	for (int i = 0; i < 4; ++i)
	{
		const std::string id = UiIds::WithIndex(UiIds::SlotRow, i);
		const std::string path = i == 0 ? "data/quicksave.ccpb" : SaveSlotPath(i);
		std::error_code ec;
		const bool filled = std::filesystem::exists(path, ec) && !ec;
		if (Rml::Element *el = ById(loadDoc_, id))
		{
			SetDisabled(el, !filled);
		}
	}
	if (Rml::Element *replay = ById(loadDoc_, UiIds::BtnReplay))
	{
		SetDisabled(replay, ReplayFrameCount(kReplayDir) <= 0);
	}
}

void RmlUiMenus::RefreshConfirm()
{
	// Texts match the confirm.rml dialog exactly.
	const bool quitting = menu_.confirm == ConfirmKind::QuitApp;
	SetTextIn(confirmDoc_, UiIds::ConfirmTitle,
			  quitting ? "Quit Conflict Converge?" : "Return to main menu?");
	SetTextIn(confirmDoc_, UiIds::ConfirmSub,
			  quitting ? "Close the game?" : "Abandon the current match?");
}
