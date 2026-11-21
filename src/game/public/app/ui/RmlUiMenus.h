#pragma once

#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>

#include "app/ui/CursorHint.h"

#include <RmlUi/Core/EventListener.h>

#include "core/MenuScreens.h"

namespace Rml {
class Context;
class Element;
class ElementDocument;
class Event;
}

class Art;
class Audio;
class EventDispatcher;
class HotkeyMap;
class RmlUiHost;
class UiList;
class UiWidget;
struct MenuFlow;

// Esc/Back stays with ShortcutBindings (game owns Esc); forwarded keys exclude
// Escape so RmlUi never double-handles it.
class RmlUiMenus : public Rml::EventListener
{
public:
	RmlUiMenus(MenuFlow &menu, HotkeyMap &hotkeys, Art &art, Audio &audio,
			   EventDispatcher &events, MenuScreens &screens, MenuCallbacks callbacks);
	RmlUiMenus(const RmlUiMenus &) = delete;
	RmlUiMenus &operator=(const RmlUiMenus &) = delete;
	~RmlUiMenus();

	// False when the host is not ready or a document is missing (caller falls back).
	bool Init(RmlUiHost &host, const std::string &dataDir);
	void Shutdown();
	bool IsReady() const { return ready_; }

	// States this owner draws; false before Init so a document failure falls back
	// instead of dereferencing a null context.
	bool HandlesState() const;
	// Hides every document; call when falling back.
	void HideAll();
	// RCSS-only restyle of every owned document: DOM, state and listeners survive.
	void ReloadStyleSheets();
	// One menu-branch frame (own Begin/EndDrawing pair); returns the confirm button.
	// HotkeyRemap routes internally to the remap screen.
	ConfirmChoice Draw(int screenWidth, int screenHeight);
	// World-branch overlay: no frame management or audio (caller owns BeginDrawing,
	// the HUD pumps the mouse). Shares the capture machine.
	void DrawRemapOverlay();
	// Menu-branch remap screen: own Begin/EndDrawing pair.
	void DrawRemapScreen(int screenWidth, int screenHeight);
	// Arms a remap capture; RML remap entries call this.
	void BeginRemap(MenuState returnTo);
	// Esc: cancel an armed capture, else back out; true when consumed.
	// ShortcutBindings prefers this while the RML remap screen owns the state.
	bool CancelRemapCapture();

	void ProcessEvent(Rml::Event &event) override;

private:
	Rml::ElementDocument *Load(const std::string &file);
	void Listen(Rml::ElementDocument *doc, const char *event,
				std::initializer_list<std::string_view> ids);
	void ShowState();
	Rml::ElementDocument *DocForState(MenuState state);
	void RefreshCurrent();
	void RefreshSetup();
	void RefreshSettings();
	void BuildMainMenuWidgets();
	void BuildSetupWidgets();
	void BuildSetupRows();
	void BuildSettingsWidgets();
	void BuildLoadWidgets();
	void BuildConfirmWidgets();
	void RefreshLoad();
	void RefreshConfirm();
	void PopulateMaps();
	void RefreshRemap();
	void PopulateRemapRows();
	void PollRemapCapture();
	std::string RemapSignature() const;
	void OnClick(const Rml::String &id);
	void Announce(EventType type);

	static void SetDisabled(Rml::Element *el, bool disabled);
	static Rml::Element *ById(Rml::ElementDocument *doc, std::string_view id);
	void SetRangeIn(Rml::ElementDocument *doc, std::string_view id, float value);
	void SetCheckIn(Rml::ElementDocument *doc, std::string_view id, bool checked);
	void SetTextIn(Rml::ElementDocument *doc, std::string_view id, const std::string &text);

	MenuFlow &menu_;
	HotkeyMap &hotkeys_;
	Art &art_;
	Audio &audio_;
	EventDispatcher &events_;
	MenuScreens &screens_;
	MenuCallbacks callbacks_;
	RmlUiHost *host_ = nullptr;
	Rml::Context *context_ = nullptr;
	std::string dataDir_;
	bool ready_ = false;
	bool shown_ = false;
	MenuState shownState_ = MenuState::MainMenu;
	bool shownConfirm_ = false;
	ConfirmChoice pendingChoice_ = ConfirmChoice::None;
	CursorHint hint_;
	std::vector<CursorHintEntry> hintEntries_;
	Rml::ElementDocument *menuDoc_ = nullptr;
	Rml::ElementDocument *setupDoc_ = nullptr;
	Rml::ElementDocument *settingsDoc_ = nullptr;
	Rml::ElementDocument *loadDoc_ = nullptr;
	Rml::ElementDocument *confirmDoc_ = nullptr;
	Rml::ElementDocument *remapDoc_ = nullptr;
	std::unique_ptr<UiWidget> mainMenuWidgets_;
	std::unique_ptr<UiWidget> setupWidgets_;
	UiList *setupRows_ = nullptr;
	std::unique_ptr<UiWidget> settingsWidgets_;
	std::unique_ptr<UiWidget> loadWidgets_;
	std::unique_ptr<UiWidget> confirmWidgets_;
	int remapArming_ = -1; // action being rebound, -1 = none
	std::string remapConflictAction_; // pending conflict, awaiting the steal click
	int remapConflictKey_ = 0;
	MenuState remapReturn_ = MenuState::Settings;
	std::string remapCache_;
	std::string mapCache_;
};
