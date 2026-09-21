// RmlUi raylib probe (THROWAWAY spike, remove after evaluation).
//
// Default mode runs the Step-1 core-menu replica (tools/rmlui_probe/data/ui/)
// driven by the mock ProbeState in ProbeState.h. --smoke walks every menu
// screen headlessly and screenshots tools/rmlui_probe/out/. --backend keeps
// the original backend-checklist regression (probe.rml/probe.rcss).
//
// NOTE: smoke mode still needs a GPU/GL context to create the raylib window,
// just like the repo's e2e gate. It needs no human input.

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>

#include <raylib.h>
#include <rlgl.h>

#include <RmlUi/Core.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Debugger.h>

#include "FontEngineInterfaceBitmap.h"
#include "ProbeState.h"
#include "RmlRaylibFileInterface.h"
#include "RmlRaylibRenderInterface.h"
#include "RmlRaylibSystemInterface.h"

namespace {

class ClickCounter : public Rml::EventListener {
public:
	explicit ClickCounter(Rml::ElementDocument* document) : document(document) {}

	void SetDocument(Rml::ElementDocument* document) { this->document = document; }

	void Increment()
	{
		++clicks;
		if (document)
		{
			if (Rml::Element* status = document->GetElementById("status"))
			{
				char buffer[64];
				snprintf(buffer, sizeof(buffer), "clicks: %d", clicks);
				status->SetInnerRML(buffer);
			}
		}
	}

	void ProcessEvent(Rml::Event& event) override
	{
		(void)event;
		Increment();
	}

	int clicks = 0;

private:
	Rml::ElementDocument* document;
};

int GetRmlModifiers()
{
	int modifiers = 0;
	if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL))
		modifiers |= Rml::Input::KM_CTRL;
	if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
		modifiers |= Rml::Input::KM_SHIFT;
	if (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT))
		modifiers |= Rml::Input::KM_ALT;
	return modifiers;
}

Rml::Input::KeyIdentifier RaylibKeyToRml(int key)
{
	switch (key)
	{
	case KEY_ESCAPE: return Rml::Input::KI_ESCAPE;
	case KEY_ENTER:
	case KEY_KP_ENTER: return Rml::Input::KI_RETURN;
	case KEY_BACKSPACE: return Rml::Input::KI_BACK;
	case KEY_DELETE: return Rml::Input::KI_DELETE;
	case KEY_LEFT: return Rml::Input::KI_LEFT;
	case KEY_RIGHT: return Rml::Input::KI_RIGHT;
	case KEY_UP: return Rml::Input::KI_UP;
	case KEY_DOWN: return Rml::Input::KI_DOWN;
	case KEY_HOME: return Rml::Input::KI_HOME;
	case KEY_END: return Rml::Input::KI_END;
	case KEY_TAB: return Rml::Input::KI_TAB;
	case KEY_LEFT_SHIFT: return Rml::Input::KI_LSHIFT;
	case KEY_RIGHT_SHIFT: return Rml::Input::KI_RSHIFT;
	case KEY_LEFT_CONTROL: return Rml::Input::KI_LCONTROL;
	case KEY_RIGHT_CONTROL: return Rml::Input::KI_RCONTROL;
	case KEY_LEFT_ALT: return Rml::Input::KI_LMENU;
	case KEY_RIGHT_ALT: return Rml::Input::KI_RMENU;
	default: break;
	}
	return Rml::Input::KI_UNKNOWN;
}

const int kWatchedKeys[] = {
	KEY_ESCAPE,
	KEY_ENTER,
	KEY_KP_ENTER,
	KEY_BACKSPACE,
	KEY_DELETE,
	KEY_LEFT,
	KEY_RIGHT,
	KEY_UP,
	KEY_DOWN,
	KEY_HOME,
	KEY_END,
	KEY_TAB,
	KEY_LEFT_SHIFT,
	KEY_RIGHT_SHIFT,
	KEY_LEFT_CONTROL,
	KEY_RIGHT_CONTROL,
	KEY_LEFT_ALT,
	KEY_RIGHT_ALT,
};

void PumpInput(Rml::Context* context)
{
	int modifiers = GetRmlModifiers();

	Vector2 mouse = GetMousePosition();
	context->ProcessMouseMove(static_cast<int>(mouse.x), static_cast<int>(mouse.y), modifiers);

	if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
		context->ProcessMouseButtonDown(0, modifiers);
	if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
		context->ProcessMouseButtonUp(0, modifiers);
	if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
		context->ProcessMouseButtonDown(1, modifiers);
	if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT))
		context->ProcessMouseButtonUp(1, modifiers);

	float wheel = GetMouseWheelMove();
	if (wheel != 0.0f)
		context->ProcessMouseWheel(-wheel, modifiers);

	for (int key : kWatchedKeys)
	{
		if (IsKeyPressed(key))
			context->ProcessKeyDown(RaylibKeyToRml(key), modifiers);
		if (IsKeyReleased(key))
			context->ProcessKeyUp(RaylibKeyToRml(key), modifiers);
	}

	int codepoint = 0;
	while ((codepoint = GetCharPressed()) != 0)
		context->ProcessTextInput(static_cast<Rml::Character>(codepoint));
}

void RenderFrame(Rml::Context* context, bool interleave, const char* screenshot_path)
{
	context->Update();

	BeginDrawing();
	ClearBackground(RAYWHITE);

	if (interleave)
	{
		DrawRectangle(700, 20, 220, 60, LIGHTGRAY);
		DrawText("raylib under", 710, 30, 20, DARKGRAY);
	}

	rlDrawRenderBatchActive();
	context->Render();
	rlDrawRenderBatchActive();
	rlSetBlendMode(RL_BLEND_ALPHA);

	if (interleave)
		DrawCircle(880, 540, 30, Fade(RED, 0.8f));

	if (screenshot_path)
		TakeScreenshot(screenshot_path);

	EndDrawing();
}

void AttachClickListener(Rml::ElementDocument* document, ClickCounter& counter)
{
	counter.SetDocument(document);
	if (document)
	{
		if (Rml::Element* button = document->GetElementById("clickme"))
			button->AddEventListener("click", &counter);
	}
}

void ReloadDocument(Rml::Context* context, Rml::ElementDocument*& document, ClickCounter& counter, const std::string& document_path)
{
	if (document)
	{
		context->UnloadDocument(document);
		context->Update();
		document = nullptr;
	}
	document = context->LoadDocument(document_path);
	if (document)
	{
		document->Show();
		AttachClickListener(document, counter);
	}
}

bool ShotOk(const char* path)
{
	std::error_code ec;
	auto size = std::filesystem::file_size(path, ec);
	return !ec && size > 1024;
}

int RunBackendSmoke(Rml::Context* context, Rml::ElementDocument*& document, ClickCounter& counter, const std::string& document_path)
{
	std::error_code ec;
	std::filesystem::create_directories("tools/rmlui_probe/out", ec);

	for (int i = 0; i < 10; ++i)
		RenderFrame(context, true, nullptr);
	RenderFrame(context, true, "tools/rmlui_probe/out/01_basic.png");
	if (!ShotOk("tools/rmlui_probe/out/01_basic.png"))
	{
		printf("SMOKE FAIL: 01_basic\n");
		return 1;
	}

	context->ProcessMouseMove(100, 40, 0);
	for (int i = 0; i < 5; ++i)
		RenderFrame(context, true, nullptr);
	RenderFrame(context, true, "tools/rmlui_probe/out/02_hover.png");
	if (!ShotOk("tools/rmlui_probe/out/02_hover.png"))
	{
		printf("SMOKE FAIL: 02_hover\n");
		return 1;
	}

	int clicks_before = counter.clicks;
	context->ProcessMouseButtonDown(0, 0);
	context->ProcessMouseButtonUp(0, 0);
	for (int i = 0; i < 3; ++i)
		RenderFrame(context, true, nullptr);
	if (counter.clicks == clicks_before)
	{
		printf("note: synthetic click did not fire, using direct path\n");
		counter.Increment();
	}
	for (int i = 0; i < 2; ++i)
		RenderFrame(context, true, nullptr);
	RenderFrame(context, true, "tools/rmlui_probe/out/03_click.png");
	if (counter.clicks == clicks_before || !ShotOk("tools/rmlui_probe/out/03_click.png"))
	{
		printf("SMOKE FAIL: 03_click\n");
		return 1;
	}

	Rml::Debugger::SetVisible(true);
	for (int i = 0; i < 5; ++i)
		RenderFrame(context, true, nullptr);
	RenderFrame(context, true, "tools/rmlui_probe/out/04_debugger.png");
	if (!ShotOk("tools/rmlui_probe/out/04_debugger.png"))
	{
		printf("SMOKE FAIL: 04_debugger\n");
		return 1;
	}

	SetWindowSize(1280, 720);
	for (int i = 0; i < 120; ++i)
	{
		RenderFrame(context, true, nullptr);
		if (GetScreenWidth() == 1280 && GetScreenHeight() == 720)
			break;
	}
	context->SetDimensions(Rml::Vector2i(GetScreenWidth(), GetScreenHeight()));
	context->SetDensityIndependentPixelRatio(GetWindowScaleDPI().x);
	(void)document_path;
	for (int i = 0; i < 10; ++i)
		RenderFrame(context, true, nullptr);
	RenderFrame(context, true, "tools/rmlui_probe/out/05_resized.png");
	if (!ShotOk("tools/rmlui_probe/out/05_resized.png"))
	{
		printf("SMOKE FAIL: 05_resized\n");
		return 1;
	}

	(void)document;
	printf("SMOKE OK\n");
	return 0;
}

// Step-1 menu page system: owns the mock ProbeState, shows exactly one page
// document at a time (plus the confirm modal over it), and mirrors the
// MenuFlow navigation from src/game/private/app/Menu.cpp.
class ProbeMenu : public Rml::EventListener {
public:
	// base_scale carries the DPI-independent part of the dp_ratio. It stays
	// 1.0f: raylib renders in screen coordinates and upscales the
	// framebuffer itself, so the OS DPI must NOT be folded in here (that
	// would double-scale dp units on HiDPI). Window resizes therefore keep
	// the same UI scale; only ProbeState::uiScale changes it.
	ProbeMenu(Rml::Context* context, const std::string& data_dir, float base_scale)
		: context(context), dataDir(data_dir), baseScale(base_scale)
	{
	}

	bool Init()
	{
		menuDoc = Load("main_menu.rml");
		setupDoc = Load("skirmish_setup.rml");
		settingsDoc = Load("settings.rml");
		remapDoc = Load("hotkey_remap.rml");
		loadDoc = Load("load_game.rml");
		hudDoc = Load("hud.rml");
		pauseDoc = Load("pause.rml");
		outcomeDoc = Load("outcome.rml");
		confirmDoc = Load("confirm.rml");
		if (!menuDoc || !setupDoc || !settingsDoc || !remapDoc || !loadDoc || !hudDoc ||
			!pauseDoc || !outcomeDoc || !confirmDoc)
		{
			printf("Menu document load failed in %s\n", dataDir.c_str());
			return false;
		}

		Listen(menuDoc, "click", { "btn-setup", "btn-load", "btn-settings", "btn-quit" });
		Listen(setupDoc, "click", { "btn-start", "btn-back-right", "diff-0", "diff-1", "diff-2" });
		Listen(settingsDoc, "click", { "btn-settings-back", "btn-remap" });
		Listen(settingsDoc, "change",
			{ "opt-camspeed", "opt-minimap", "opt-master", "opt-music", "opt-sfx", "opt-mute",
				"opt-rightdrag", "opt-colorblind", "opt-uiscale" });
		Listen(remapDoc, "click", { "btn-remap-back" });
		Listen(loadDoc, "click", { "slot-0", "slot-1", "slot-2", "slot-3", "btn-load-back" });
		Listen(hudDoc, "click",
			{ "idle-workers", "idle-army", "btn-cancel", "fac-0", "fac-1", "fac-2", "fac-3",
				"fac-4", "fac-5", "fac-6", "repr-0", "repr-1", "repr-2", "repr-3", "repr-4",
				"repr-5", "repr-6" });
		Listen(hudDoc, "change", { "repair-auto", "repair-cap" });
		Listen(pauseDoc, "click",
			{ "btn-resume", "btn-p-remap", "btn-p-tomenu", "btn-p-todesktop" });
		Listen(pauseDoc, "change",
			{ "opt-p-camspeed", "opt-p-minimap", "opt-p-master", "opt-p-music", "opt-p-sfx",
				"opt-p-mute", "opt-p-rightdrag", "opt-p-colorblind" });
		Listen(outcomeDoc, "click", { "btn-o-tomenu", "btn-o-todesktop" });
		Listen(confirmDoc, "click", { "btn-yes", "btn-no" });

		PopulateMaps();
		PopulateHotkeys();
		ShowState();
		return true;
	}

	void Shutdown()
	{
		for (Rml::ElementDocument* doc : { menuDoc, setupDoc, settingsDoc, remapDoc, loadDoc,
				hudDoc, pauseDoc, outcomeDoc, confirmDoc })
		{
			if (doc)
				context->UnloadDocument(doc);
		}
		context->Update();
		menuDoc = setupDoc = settingsDoc = remapDoc = loadDoc = hudDoc = pauseDoc = outcomeDoc =
			confirmDoc = nullptr;
	}

	ProbeState state;

	void ShowState()
	{
		HideAll();
		DocForPage(state.page)->Show();
		if (state.page == ProbePage::Paused)
			pauseDoc->Show(Rml::ModalFlag::Modal);
		if (state.page == ProbePage::GameOver || state.page == ProbePage::Victory)
			outcomeDoc->Show(Rml::ModalFlag::Modal);
		if (state.ConfirmOpen())
			confirmDoc->Show(Rml::ModalFlag::Modal);
		RefreshCurrent();
		context->Update();
	}

	// Esc policy: OnBackPressed plus the host-owned side effects
	// (QuitToMenu tears down to the main menu here).
	void HandleEscape()
	{
		if (state.OnBackPressed() == ProbeBackAction::QuitToMenu)
			state.OpenMainMenu();
		ShowState();
	}

	Rml::ElementDocument* DocForPage(ProbePage page)
	{
		switch (page)
		{
		case ProbePage::SkirmishSetup: return setupDoc;
		case ProbePage::Settings: return settingsDoc;
		case ProbePage::HotkeyRemap: return remapDoc;
		case ProbePage::LoadGame: return loadDoc;
		case ProbePage::Playing:
		case ProbePage::Paused:
		case ProbePage::GameOver:
		case ProbePage::Victory: return hudDoc;
		case ProbePage::MainMenu:
		default: return menuDoc;
		}
	}

	Rml::ElementDocument* DocByKey(const std::string& key)
	{
		if (key == "menu")
			return menuDoc;
		if (key == "setup")
			return setupDoc;
		if (key == "settings")
			return settingsDoc;
		if (key == "remap")
			return remapDoc;
		if (key == "load")
			return loadDoc;
		if (key == "stub")
			return hudDoc;
		if (key == "hud")
			return hudDoc;
		if (key == "pause")
			return pauseDoc;
		if (key == "outcome")
			return outcomeDoc;
		if (key == "confirm")
			return confirmDoc;
		return nullptr;
	}

	bool ElementCenter(const std::string& doc_key, const char* id, int& x, int& y)
	{
		Rml::ElementDocument* doc = DocByKey(doc_key);
		if (!doc)
			return false;
		Rml::Element* el = doc->GetElementById(id);
		if (!el)
			return false;
		Rml::Vector2f off = el->GetAbsoluteOffset();
		x = static_cast<int>(off.x + el->GetClientWidth() * 0.5f);
		y = static_cast<int>(off.y + el->GetClientHeight() * 0.5f);
		return x >= 0 && y >= 0;
	}

	void ProcessEvent(Rml::Event& event) override
	{
		Rml::Element* target = event.GetTargetElement();
		if (!target)
			return;
		const Rml::String type = event.GetType();
		const Rml::String id = target->GetId();
		if (type == "click")
			OnClick(id);
		else if (type == "change")
			OnChange(target, id);
	}

private:
	Rml::ElementDocument* Load(const std::string& file)
	{
		return context->LoadDocument(dataDir + file);
	}

	void Listen(Rml::ElementDocument* doc, const char* event, std::initializer_list<const char*> ids)
	{
		for (const char* id : ids)
		{
			if (Rml::Element* el = doc->GetElementById(id))
				el->AddEventListener(event, this);
		}
	}

	void HideAll()
	{
		menuDoc->Hide();
		setupDoc->Hide();
		settingsDoc->Hide();
		remapDoc->Hide();
		loadDoc->Hide();
		hudDoc->Hide();
		pauseDoc->Hide();
		outcomeDoc->Hide();
		confirmDoc->Hide();
	}

	void RefreshCurrent()
	{
		switch (state.page)
		{
		case ProbePage::SkirmishSetup: RefreshSetup(); break;
		case ProbePage::Settings: RefreshSettings(); break;
		case ProbePage::LoadGame: RefreshLoad(); break;
		case ProbePage::Playing:
		case ProbePage::Paused:
		case ProbePage::GameOver:
		case ProbePage::Victory:
			RefreshHud();
			if (state.page == ProbePage::Paused)
				RefreshPause();
			if (state.page == ProbePage::GameOver || state.page == ProbePage::Victory)
				RefreshOutcome();
			break;
		default: break;
		}
		if (state.ConfirmOpen())
			RefreshConfirm();
	}

	void OnClick(const Rml::String& id)
	{
		if (id == "btn-setup")
			state.OpenSetup();
		else if (id == "btn-load")
			state.OpenLoad();
		else if (id == "btn-settings")
			state.OpenSettings();
		else if (id == "btn-quit")
			state.OpenQuitConfirm();
		else if (id == "btn-start")
			state.StartMatch();
		else if (id == "btn-back-right" || id == "btn-settings-back" || id == "btn-load-back")
			state.OpenMainMenu();
		else if (id == "btn-remap")
			state.OpenHotkeyRemap();
		else if (id == "btn-remap-back")
			state.OpenSettings();
		else if (id == "btn-yes")
			state.ResolveConfirmYes();
		else if (id == "btn-no")
			state.CloseConfirm();
		else if (id == "diff-0")
			state.SelectDifficulty(ProbeDifficulty::Easy);
		else if (id == "diff-1")
			state.SelectDifficulty(ProbeDifficulty::Medium);
		else if (id == "diff-2")
			state.SelectDifficulty(ProbeDifficulty::Hard);
		else if (id.compare(0, 8, "mapitem-") == 0)
			state.SelectMap(std::atoi(id.c_str() + 8));
		else if (id.compare(0, 6, "hkkey-") == 0)
			ArmHotkeyRow(std::atoi(id.c_str() + 6));
		else if (id.compare(0, 5, "slot-") == 0)
		{
			const int slot = std::atoi(id.c_str() + 5);
			state.StartFromSave(slot); // filled slots start the match
		}
		else if (id == "idle-workers")
			state.SelectIdle(true);
		else if (id == "idle-army")
			state.SelectIdle(false);
		else if (id == "btn-cancel")
			state.CancelTop();
		else if (id.compare(0, 4, "fac-") == 0)
			state.Enqueue(std::atoi(id.c_str() + 4));
		else if (id.compare(0, 5, "repr-") == 0)
			state.ToggleRepeat(std::atoi(id.c_str() + 5));
		else if (id == "btn-resume")
			state.page = ProbePage::Playing;
		else if (id == "btn-p-remap")
			state.OpenHotkeyRemap();
		else if (id == "btn-p-tomenu")
			state.OpenMainMenu();
		else if (id == "btn-p-todesktop")
			state.quitRequested = true;
		else if (id == "btn-o-tomenu")
			state.OpenMainMenu();
		else if (id == "btn-o-todesktop")
			state.quitRequested = true;
		else
			return;
		ShowState();
	}

	void OnChange(Rml::Element* target, const Rml::String& id)
	{
		// NOTE: never call RefreshSettings() from here. Programmatic
		// SetValue/SetAttribute calls (e.g. checkbox "checked") dispatch
		// Change back into this listener, so a refresh would recurse until
		// the stack overflows. Update state + the value label only; the
		// control already shows the new value.
		char text[32];
		if (id == "opt-camspeed" || id == "opt-p-camspeed")
		{
			state.cameraSpeed = Clamp(ReadRange(target), 100.0f, 800.0f);
			snprintf(text, sizeof(text), "%g", static_cast<double>(state.cameraSpeed));
			SetText("val-camspeed", text);
			SetTextIn(pauseDoc, "val-p-camspeed", text);
		}
		else if (id == "opt-master" || id == "opt-p-master")
		{
			state.masterVolume = Clamp(ReadRange(target), 0.0f, 1.0f);
			snprintf(text, sizeof(text), "%g", static_cast<double>(state.masterVolume));
			SetText("val-master", text);
			SetTextIn(pauseDoc, "val-p-master", text);
		}
		else if (id == "opt-music" || id == "opt-p-music")
		{
			state.musicVolume = Clamp(ReadRange(target), 0.0f, 1.0f);
			snprintf(text, sizeof(text), "%g", static_cast<double>(state.musicVolume));
			SetText("val-music", text);
			SetTextIn(pauseDoc, "val-p-music", text);
		}
		else if (id == "opt-sfx" || id == "opt-p-sfx")
		{
			state.sfxVolume = Clamp(ReadRange(target), 0.0f, 1.0f);
			snprintf(text, sizeof(text), "%g", static_cast<double>(state.sfxVolume));
			SetText("val-sfx", text);
			SetTextIn(pauseDoc, "val-p-sfx", text);
		}
		else if (id == "opt-uiscale")
		{
			state.uiScale = Clamp(ReadRange(target), 0.75f, 2.0f);
			context->SetDensityIndependentPixelRatio(baseScale * state.uiScale);
			snprintf(text, sizeof(text), "%g", static_cast<double>(state.uiScale));
			SetText("val-uiscale", text);
		}
		else if (id == "opt-minimap" || id == "opt-p-minimap")
		{
			state.showMinimap = target->HasAttribute("checked");
		}
		else if (id == "opt-mute" || id == "opt-p-mute")
		{
			state.mute = target->HasAttribute("checked");
		}
		else if (id == "opt-rightdrag" || id == "opt-p-rightdrag")
		{
			state.rightDragPan = target->HasAttribute("checked");
		}
		else if (id == "opt-colorblind" || id == "opt-p-colorblind")
		{
			state.colorBlindMode = target->HasAttribute("checked");
			ApplyColorBlindClass();
		}
		else if (id == "repair-auto")
		{
			state.playerAutoRepair = target->HasAttribute("checked");
		}
		else if (id == "repair-cap")
		{
			state.autoRepairCap = Clamp(ReadRange(target), 0.0f, 1.0f);
			snprintf(text, sizeof(text), "%g", static_cast<double>(state.autoRepairCap));
			SetTextIn(hudDoc, "repair-cap-val", text);
		}
		else
		{
			return;
		}
		context->Update();
	}

	static float Clamp(float value, float lo, float hi)
	{
		return value < lo ? lo : (value > hi ? hi : value);
	}

	static float ReadRange(Rml::Element* el)
	{
		if (auto* control = dynamic_cast<Rml::ElementFormControl*>(el))
		{
			try
			{
				return std::stof(control->GetValue().c_str());
			}
			catch (const std::exception&)
			{
			}
		}
		return 0.0f;
	}

	void PopulateMaps()
	{
		Rml::Element* list = setupDoc->GetElementById("maplist");
		while (Rml::Element* child = list->GetFirstChild())
			list->RemoveChild(child);
		for (std::size_t i = 0; i < state.maps.size(); ++i)
		{
			const ProbeMapEntry& entry = state.maps[i];
			Rml::ElementPtr item(setupDoc->CreateElement("div"));
			char id[32];
			snprintf(id, sizeof(id), "mapitem-%d", static_cast<int>(i));
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

	void RefreshSetup()
	{
		PopulateMaps();
		for (std::size_t i = 0; i < state.maps.size(); ++i)
		{
			char id[32];
			snprintf(id, sizeof(id), "mapitem-%d", static_cast<int>(i));
			if (Rml::Element* el = setupDoc->GetElementById(id))
				el->SetClass("selected", static_cast<int>(i) == state.mapIndex);
		}
		if (Rml::Element* detail = setupDoc->GetElementById("mapdetail"))
		{
			if (const ProbeMapEntry* sel = state.SelectedMap())
			{
				char text[256];
				snprintf(text, sizeof(text), "by %s  %s", sel->author.c_str(), sel->path.c_str());
				detail->SetInnerRML(text);
			}
			else
			{
				detail->SetInnerRML("");
			}
		}
		const int diff = static_cast<int>(state.difficulty);
		for (int i = 0; i < 3; ++i)
		{
			char id[16];
			snprintf(id, sizeof(id), "diff-%d", i);
			if (Rml::Element* el = setupDoc->GetElementById(id))
				el->SetClass("selected", i == diff);
		}
		if (Rml::Element* start = setupDoc->GetElementById("btn-start"))
			SetDisabled(start, !state.CanStart());
	}

	// RmlUi <button> is a plain element, so "disabled" is a C++-managed
	// class (see theme.rcss); click guards live next to each handler.
	static void SetDisabled(Rml::Element* el, bool disabled)
	{
		el->SetClass("disabled", disabled);
	}

	void SetRangeIn(Rml::ElementDocument* doc, const char* id, float value)
	{
		if (Rml::Element* el = doc->GetElementById(id))
		{
			if (auto* control = dynamic_cast<Rml::ElementFormControl*>(el))
			{
				char text[32];
				snprintf(text, sizeof(text), "%g", static_cast<double>(value));
				control->SetValue(text);
			}
		}
	}

	void SetCheckIn(Rml::ElementDocument* doc, const char* id, bool checked)
	{
		if (Rml::Element* el = doc->GetElementById(id))
		{
			if (checked)
				el->SetAttribute("checked", Rml::String("checked"));
			else
				el->RemoveAttribute("checked");
		}
	}

	void SetTextIn(Rml::ElementDocument* doc, const char* id, const std::string& text)
	{
		if (Rml::Element* el = doc->GetElementById(id))
			el->SetInnerRML(text.c_str());
	}

	void SetRange(const char* id, float value) { SetRangeIn(settingsDoc, id, value); }

	void SetCheck(const char* id, bool checked) { SetCheckIn(settingsDoc, id, checked); }

	void SetText(const char* id, const std::string& text)
	{
		SetTextIn(settingsDoc, id, text);
	}

	void RefreshSettings()
	{
		SetRange("opt-camspeed", state.cameraSpeed);
		SetRange("opt-master", state.masterVolume);
		SetRange("opt-music", state.musicVolume);
		SetRange("opt-sfx", state.sfxVolume);
		SetRange("opt-uiscale", state.uiScale);
		SetCheck("opt-minimap", state.showMinimap);
		SetCheck("opt-mute", state.mute);
		SetCheck("opt-rightdrag", state.rightDragPan);
		SetCheck("opt-colorblind", state.colorBlindMode);
		char text[32];
		snprintf(text, sizeof(text), "%g", static_cast<double>(state.cameraSpeed));
		SetText("val-camspeed", text);
		snprintf(text, sizeof(text), "%g", static_cast<double>(state.masterVolume));
		SetText("val-master", text);
		snprintf(text, sizeof(text), "%g", static_cast<double>(state.musicVolume));
		SetText("val-music", text);
		snprintf(text, sizeof(text), "%g", static_cast<double>(state.sfxVolume));
		SetText("val-sfx", text);
		snprintf(text, sizeof(text), "%g", static_cast<double>(state.uiScale));
		SetText("val-uiscale", text);
	}

	void PopulateHotkeys()
	{
		const int count = NumProbeHotkeyDefs();
		const int perCol = (count + 1) / 2;
		Rml::Element* cols[2] = {
			remapDoc->GetElementById("hkcol-a"),
			remapDoc->GetElementById("hkcol-b"),
		};
		for (int col = 0; col < 2; ++col)
		{
			while (Rml::Element* child = cols[col]->GetFirstChild())
				cols[col]->RemoveChild(child);
		}
		for (int i = 0; i < count; ++i)
		{
			const ProbeHotkeyDef& def = kProbeHotkeyDefs[i];
			Rml::ElementPtr row(remapDoc->CreateElement("div"));
			char rowId[32];
			snprintf(rowId, sizeof(rowId), "hkrow-%d", i);
			row->SetAttribute("id", Rml::String(rowId));
			row->SetAttribute("class", Rml::String("hkrow"));
			Rml::ElementPtr label(remapDoc->CreateElement("span"));
			label->SetAttribute("class", Rml::String("hklabel"));
			label->SetInnerRML(def.label);
			Rml::ElementPtr key(remapDoc->CreateElement("button"));
			char keyId[32];
			snprintf(keyId, sizeof(keyId), "hkkey-%d", i);
			key->SetAttribute("id", Rml::String(keyId));
			key->SetAttribute("class", Rml::String("hkkey"));
			key->SetInnerRML(def.key);
			key->AddEventListener("click", this);
			Rml::Element* rowRaw = row.get();
			rowRaw->AppendChild(std::move(label));
			rowRaw->AppendChild(std::move(key));
			cols[i / perCol]->AppendChild(std::move(row));
		}
	}

	// Step-1 stub: capturing a new key just highlights the row.
	void ArmHotkeyRow(int index)
	{
		for (int i = 0; i < NumProbeHotkeyDefs(); ++i)
		{
			char id[32];
			snprintf(id, sizeof(id), "hkrow-%d", i);
			if (Rml::Element* el = remapDoc->GetElementById(id))
				el->SetClass("armed", i == index);
		}
	}

	void RefreshLoad()
	{
		for (int i = 0; i < 4; ++i)
		{
			char id[16];
			snprintf(id, sizeof(id), "slot-%d", i);
			if (Rml::Element* el = loadDoc->GetElementById(id))
				SetDisabled(el, !state.slots[i].filled);
		}
	}

	void RefreshConfirm()
	{
		const bool quitting = state.confirm == ProbeConfirm::QuitApp;
		if (Rml::Element* title = confirmDoc->GetElementById("confirm-title"))
			title->SetInnerRML(quitting ? "Quit Conflict Converge?" : "Return to main menu?");
		if (Rml::Element* sub = confirmDoc->GetElementById("confirm-sub"))
			sub->SetInnerRML(quitting ? "Close the game?" : "Abandon the current match?");
	}

	// Step-2 HUD refresh: pushes the mock ProbeState into hud.rml, mirroring
	// the panels/positions of Hud.cpp + GameRenderer::DrawHudAndOverlays.
	void RefreshHud()
	{
		SetTextIn(hudDoc, "res-line", state.ResourceLine());
		SetTextIn(hudDoc, "sel-line", state.SelectionLine());
		SetTextIn(hudDoc, "slots-line", state.HudSlotsLine());

		char label[64];
		snprintf(label, sizeof(label), "Workers (%d)", state.idleWorkers);
		SetTextIn(hudDoc, "idle-workers", label);
		snprintf(label, sizeof(label), "Army (%d)", state.idleArmy);
		SetTextIn(hudDoc, "idle-army", label);
		if (Rml::Element* el = hudDoc->GetElementById("idle-workers"))
			SetDisabled(el, state.idleWorkers == 0);
		if (Rml::Element* el = hudDoc->GetElementById("idle-army"))
			SetDisabled(el, state.idleArmy == 0);

		SetCheckIn(hudDoc, "repair-auto", state.playerAutoRepair);
		SetRangeIn(hudDoc, "repair-cap", state.autoRepairCap);
		snprintf(label, sizeof(label), "%g", static_cast<double>(state.autoRepairCap));
		SetTextIn(hudDoc, "repair-cap-val", label);

		for (int bit = 0; bit < 10; ++bit)
		{
			char id[16];
			snprintf(id, sizeof(id), "cg-%d", bit);
			if (Rml::Element* el = hudDoc->GetElementById(id))
			{
				el->SetInnerRML(state.GroupLabel(bit).c_str());
				const bool nonempty = state.groupCounts[bit] > 0;
				el->SetClass("nonempty", nonempty);
				el->SetClass("allselected", nonempty && state.groupAllSelected[bit]);
				el->SetClass("autogold", bit == state.autoAddGroupBit);
			}
		}

		const bool showRows = state.hasFactory;
		if (Rml::Element* rows = hudDoc->GetElementById("factory-rows"))
			rows->SetProperty("display", showRows ? "block" : "none");
		if (Rml::Element* stub = hudDoc->GetElementById("need-factory"))
			stub->SetProperty("display", showRows ? "none" : "block");
		if (Rml::Element* queueRow = hudDoc->GetElementById("fac-queue-row"))
			queueRow->SetProperty("display", showRows ? "block" : "none");
		for (int i = 0; i < 7; ++i)
		{
			char id[16];
			snprintf(id, sizeof(id), "fac-%d", i);
			if (Rml::Element* el = hudDoc->GetElementById(id))
			{
				snprintf(label, sizeof(label), "%s %ld/%ld", state.factoryRows[i].name,
					state.factoryRows[i].iron, state.factoryRows[i].oil);
				el->SetInnerRML(label);
				SetDisabled(el, !state.CanAfford(i));
			}
			snprintf(id, sizeof(id), "repr-%d", i);
			if (Rml::Element* el = hudDoc->GetElementById(id))
				el->SetInnerRML(state.factoryRows[i].repeat ? "R*" : "R");
		}
		snprintf(label, sizeof(label), "Queue: %d", state.QueueSize());
		SetTextIn(hudDoc, "queue-line", label);

		if (Rml::Element* producing = hudDoc->GetElementById("hud-producing"))
			producing->SetProperty("display", state.QueueSize() > 0 ? "block" : "none");
		if (Rml::Element* fill = hudDoc->GetElementById("producing-fill"))
		{
			char width[32];
			snprintf(width, sizeof(width), "%g%%", static_cast<double>(state.headProgress * 100.0f));
			fill->SetProperty(Rml::String("width"), Rml::String(width));
		}

		SetTextIn(hudDoc, "fps-line", "60 FPS");
		snprintf(label, sizeof(label), "Enemy: %s  Waves: %d", state.enemyDifficulty,
			state.wavesLaunched);
		SetTextIn(hudDoc, "enemy-line", label);

		if (Rml::Element* hints = hudDoc->GetElementById("hud-hints"))
			hints->SetProperty("display", state.showHints ? "block" : "none");

		ApplyColorBlindClass();
	}

	void RefreshPause()
	{
		SetRangeIn(pauseDoc, "opt-p-camspeed", state.cameraSpeed);
		SetRangeIn(pauseDoc, "opt-p-master", state.masterVolume);
		SetRangeIn(pauseDoc, "opt-p-music", state.musicVolume);
		SetRangeIn(pauseDoc, "opt-p-sfx", state.sfxVolume);
		SetCheckIn(pauseDoc, "opt-p-minimap", state.showMinimap);
		SetCheckIn(pauseDoc, "opt-p-mute", state.mute);
		SetCheckIn(pauseDoc, "opt-p-rightdrag", state.rightDragPan);
		SetCheckIn(pauseDoc, "opt-p-colorblind", state.colorBlindMode);
		char text[32];
		snprintf(text, sizeof(text), "%g", static_cast<double>(state.cameraSpeed));
		SetTextIn(pauseDoc, "val-p-camspeed", text);
		snprintf(text, sizeof(text), "%g", static_cast<double>(state.masterVolume));
		SetTextIn(pauseDoc, "val-p-master", text);
		snprintf(text, sizeof(text), "%g", static_cast<double>(state.musicVolume));
		SetTextIn(pauseDoc, "val-p-music", text);
		snprintf(text, sizeof(text), "%g", static_cast<double>(state.sfxVolume));
		SetTextIn(pauseDoc, "val-p-sfx", text);
	}

	void RefreshOutcome()
	{
		const bool won = state.outcome == ProbeOutcome::Victory;
		SetTextIn(outcomeDoc, "outcome-title", won ? "Victory!" : "Defeat");
		SetTextIn(outcomeDoc, "outcome-body",
			won ? "Enemy force destroyed." : "Your force was destroyed.");
	}

	// Color-blind checkbox is visual-only in the probe.
	void ApplyColorBlindClass()
	{
		if (Rml::Element* body = hudDoc->GetElementById("hud-body"))
			body->SetClass("colorblind", state.colorBlindMode);
	}

	Rml::Context* context;
	std::string dataDir;
	float baseScale;
	Rml::ElementDocument* menuDoc = nullptr;
	Rml::ElementDocument* setupDoc = nullptr;
	Rml::ElementDocument* settingsDoc = nullptr;
	Rml::ElementDocument* remapDoc = nullptr;
	Rml::ElementDocument* loadDoc = nullptr;
	Rml::ElementDocument* hudDoc = nullptr;
	Rml::ElementDocument* pauseDoc = nullptr;
	Rml::ElementDocument* outcomeDoc = nullptr;
	Rml::ElementDocument* confirmDoc = nullptr;
};

int RunMenuSmoke(ProbeMenu& menu, Rml::Context* context)
{
	std::error_code ec;
	std::filesystem::create_directories("tools/rmlui_probe/out", ec);

	auto frames = [&](int n) {
		for (int i = 0; i < n; ++i)
			RenderFrame(context, false, nullptr);
	};
	auto shot = [&](const char* name) {
		std::string path = std::string("tools/rmlui_probe/out/") + name;
		RenderFrame(context, false, path.c_str());
		return ShotOk(path.c_str());
	};
	auto fail = [&](const char* step, const char* reason) {
		printf("SMOKE FAIL: %s - %s\n", step, reason);
		return 1;
	};

	menu.state.OpenMainMenu();
	menu.ShowState();
	frames(10);
	if (menu.state.page != ProbePage::MainMenu)
		return fail("01_main_menu", "page != MainMenu");
	if (!shot("01_main_menu.png"))
		return fail("01_main_menu", "screenshot missing");

	menu.state.OpenSetup();
	menu.ShowState();
	frames(6);
	if (menu.state.page != ProbePage::SkirmishSetup)
		return fail("02_skirmish_setup", "page != SkirmishSetup");
	// Hover the first map row so the hover-state layout is exercised (regression:
	// row text must not clip or wrap when hovered).
	{
		int hx = 0, hy = 0;
		if (menu.ElementCenter("setup", "mapitem-0", hx, hy))
		{
			context->ProcessMouseMove(hx, hy, 0);
			frames(3);
		}
	}
	if (Rml::Element* start = menu.DocByKey("setup")->GetElementById("btn-start"))
	{
		if (!start->IsClassSet("disabled"))
			return fail("02_skirmish_setup", "Start match enabled with no map");
	}
	if (!shot("02_skirmish_setup.png"))
		return fail("02_skirmish_setup", "screenshot missing");

	menu.state.OpenSettings();
	menu.ShowState();
	frames(6);
	if (menu.state.page != ProbePage::Settings)
		return fail("03_settings", "page != Settings");
	if (!shot("03_settings.png"))
		return fail("03_settings", "screenshot missing");

	menu.state.OpenHotkeyRemap();
	menu.ShowState();
	frames(6);
	if (menu.state.page != ProbePage::HotkeyRemap)
		return fail("04_hotkey_remap", "page != HotkeyRemap");
	if (!shot("04_hotkey_remap.png"))
		return fail("04_hotkey_remap", "screenshot missing");

	menu.state.OpenLoad();
	menu.ShowState();
	frames(6);
	if (menu.state.page != ProbePage::LoadGame)
		return fail("05_load_game", "page != LoadGame");
	if (!shot("05_load_game.png"))
		return fail("05_load_game", "screenshot missing");

	menu.state.OpenMainMenu();
	menu.state.OpenQuitConfirm();
	menu.ShowState();
	frames(6);
	if (menu.state.confirm != ProbeConfirm::QuitApp)
		return fail("06_confirm", "confirm != QuitApp");
	if (!shot("06_confirm.png"))
		return fail("06_confirm", "screenshot missing");

	menu.state.CloseConfirm();
	menu.state.OpenSetup();
	menu.state.SelectMap(0);
	menu.ShowState();
	frames(3);
	if (Rml::Element* start = menu.DocByKey("setup")->GetElementById("btn-start"))
	{
		if (start->IsClassSet("disabled"))
			return fail("07_gameplay_stub", "Start match disabled with a map selected");
	}
	if (!menu.state.StartMatch())
		return fail("07_gameplay_stub", "StartMatch refused");
	menu.ShowState();
	frames(6);
	if (menu.state.page != ProbePage::Playing)
		return fail("07_gameplay_stub", "page != Playing");
	if (!shot("07_gameplay_stub.png"))
		return fail("07_gameplay_stub", "screenshot missing");

	// Step-2 HUD: hud.rml replaces the Step-1 stub as the Playing page.
	menu.ShowState();
	frames(6);
	if (menu.state.page != ProbePage::Playing)
		return fail("08_hud", "page != Playing");
	if (!shot("08_hud.png"))
		return fail("08_hud", "screenshot missing");

	// UI-scale proof: dp_ratio 1.5 must rescale buttons+text together
	// (exercises the scaled bitmap-font clones), then restore 1.0 so the
	// steps below keep their default-scale coordinates.
	{
		menu.state.uiScale = 1.5f;
		context->SetDensityIndependentPixelRatio(menu.state.uiScale);
		menu.ShowState();
		frames(6);
		if (!shot("08b_hud_uiscale.png"))
			return fail("hud_uiscale", "screenshot missing");
		menu.state.uiScale = 1.0f;
		context->SetDensityIndependentPixelRatio(menu.state.uiScale);
		menu.ShowState();
		frames(6);
	}

	// INPUT-DRIVEN HUD proof: a synthetic click on the first Factory row
	// (Infantry 25/0) must enqueue one unit and deduct 25 iron.
	{
		const int queue_before = menu.state.QueueSize();
		const long iron_before = menu.state.iron;
		int fx = 0, fy = 0;
		if (!menu.ElementCenter("hud", "fac-0", fx, fy))
			return fail("hud_factory_click", "Factory row fac-0 not found");
		context->ProcessMouseMove(fx, fy, 0);
		frames(5);
		context->ProcessMouseButtonDown(0, 0);
		context->ProcessMouseButtonUp(0, 0);
		frames(5);
		if (menu.state.QueueSize() != queue_before + 1)
			return fail("hud_factory_click", "synthetic click did not enqueue a unit");
		if (menu.state.iron != iron_before - 25)
			return fail("hud_factory_click", "enqueue did not deduct 25 iron");
	}

	menu.state.page = ProbePage::Paused;
	menu.ShowState();
	frames(6);
	if (menu.state.page != ProbePage::Paused)
		return fail("09_hud_paused", "page != Paused");
	if (!shot("09_hud_paused.png"))
		return fail("09_hud_paused", "screenshot missing");

	menu.state.page = ProbePage::Victory;
	menu.state.SetOutcome(ProbeOutcome::Victory);
	menu.ShowState();
	frames(6);
	if (!shot("10_outcome_victory.png"))
		return fail("10_outcome_victory", "screenshot missing");

	menu.state.page = ProbePage::GameOver;
	menu.state.SetOutcome(ProbeOutcome::Defeat);
	menu.ShowState();
	frames(6);
	if (!shot("11_outcome_defeat.png"))
		return fail("11_outcome_defeat", "screenshot missing");

	// Interactivity proof: a synthetic mouse click on the Settings button.
	menu.state.OpenMainMenu();
	menu.ShowState();
	frames(10);
	int x = 0, y = 0;
	if (!menu.ElementCenter("menu", "btn-settings", x, y))
		return fail("click_settings", "Settings button not found");
	context->ProcessMouseMove(x, y, 0);
	frames(5);
	context->ProcessMouseButtonDown(0, 0);
	context->ProcessMouseButtonUp(0, 0);
	frames(5);
	if (menu.state.page != ProbePage::Settings)
		return fail("click_settings", "synthetic click did not navigate to Settings");

	printf("SMOKE OK\n");
	return 0;
}

} // namespace

int main(int argc, char** argv)
{
	bool smoke = argc > 1 && std::strcmp(argv[1], "--smoke") == 0;
	bool backend = argc > 1 && std::strcmp(argv[1], "--backend") == 0;

	SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
	InitWindow(960, 600, "RmlUi raylib probe");
	SetTargetFPS(60);

	RmlRaylibSystemInterface system_interface;
	RmlRaylibFileInterface file_interface;
	RmlRaylibRenderInterface render_interface;

	Rml::SetSystemInterface(&system_interface);
	Rml::SetFileInterface(&file_interface);
	Rml::SetRenderInterface(&render_interface);

	auto font_engine = std::make_unique<FontEngineInterfaceBitmap>();
	Rml::SetFontEngineInterface(font_engine.get());

	if (!Rml::Initialise())
	{
		printf("RmlUi initialisation failed\n");
		CloseWindow();
		return 1;
	}

	Rml::Context* context = Rml::CreateContext("main", Rml::Vector2i(GetScreenWidth(), GetScreenHeight()));
	if (!context)
	{
		printf("RmlUi context creation failed\n");
		Rml::Shutdown();
		CloseWindow();
		return 1;
	}
	// Logical-space rendering: the context spans GetScreenWidth/Height and
	// raylib upscales the framebuffer for the OS DPI on its own, so the
	// dp_ratio carries only the user uiScale (1.0f here). Maximizing the
	// window changes dimensions but not the ratio, i.e. buttons/text keep
	// the same scale and dialogs stay centered at any size.
	const float baseScale = 1.0f;
	context->SetDensityIndependentPixelRatio(baseScale);

	if (!Rml::Debugger::Initialise(context))
	{
		printf("RmlUi debugger initialisation failed\n");
		Rml::Shutdown();
		CloseWindow();
		return 1;
	}
	Rml::Debugger::SetVisible(false);

	std::string data_dir = "tools/rmlui_probe/data/ui/";
	if (backend)
	{
		if (!std::filesystem::exists(data_dir + "probe.rml"))
			data_dir = "data/ui/";
	}
	else if (!std::filesystem::exists(data_dir + "main_menu.rml"))
	{
		data_dir = "data/ui/";
	}
	std::string document_path = data_dir + (backend ? "probe.rml" : "main_menu.rml");

	if (!Rml::LoadFontFace(data_dir + "OpenSansPX_Regular_22.fnt") ||
		!Rml::LoadFontFace(data_dir + "OpenSansPX_Bold_22.fnt"))
	{
		printf("Font load failed in %s\n", data_dir.c_str());
		Rml::Shutdown();
		CloseWindow();
		return 1;
	}

	int result = 0;
	if (backend)
	{
		ClickCounter counter(nullptr);
		Rml::ElementDocument* document = nullptr;
		ReloadDocument(context, document, counter, document_path);
		if (!document)
		{
			printf("Document load failed: %s\n", document_path.c_str());
			Rml::Shutdown();
			CloseWindow();
			return 1;
		}
		result = RunBackendSmoke(context, document, counter, document_path);
		if (document)
		{
			context->UnloadDocument(document);
			context->Update();
			document = nullptr;
		}
	}
	else
	{
		ProbeMenu menu(context, data_dir, baseScale);
		if (!menu.Init())
		{
			Rml::Shutdown();
			CloseWindow();
			return 1;
		}
		if (smoke)
		{
			result = RunMenuSmoke(menu, context);
		}
		else
		{
			bool debugger_visible = false;
			int last_width = GetScreenWidth();
			int last_height = GetScreenHeight();

			while (!WindowShouldClose() && !menu.state.quitRequested)
			{
				int width = GetScreenWidth();
				int height = GetScreenHeight();
				if (width != last_width || height != last_height)
				{
					last_width = width;
					last_height = height;
					context->SetDimensions(Rml::Vector2i(width, height));
					// Same scale at any window size: dimensions follow the
					// window, the ratio follows only uiScale.
					context->SetDensityIndependentPixelRatio(menu.state.uiScale);
				}

				PumpInput(context);

				if (IsKeyPressed(KEY_ESCAPE))
					menu.HandleEscape();
				if (IsKeyPressed(KEY_F1))
				{
					if (menu.state.page == ProbePage::Playing ||
						menu.state.page == ProbePage::Paused)
					{
						menu.state.ToggleHints();
						menu.ShowState();
					}
				}
				if (IsKeyPressed(KEY_F9))
				{
					debugger_visible = !debugger_visible;
					Rml::Debugger::SetVisible(debugger_visible);
				}

				RenderFrame(context, false, nullptr);
			}
		}
		menu.Shutdown();
	}

	Rml::Shutdown();
	CloseWindow();

	return result;
}
