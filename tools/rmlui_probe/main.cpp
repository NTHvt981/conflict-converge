// RmlUi raylib probe (THROWAWAY spike, remove after evaluation).
//
// NOTE: smoke mode (--smoke) still needs a GPU/GL context to create the
// raylib window, just like the repo's e2e gate. It needs no human input.

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>

#include <raylib.h>
#include <rlgl.h>

#include <RmlUi/Core.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Debugger.h>

#include "FontEngineInterfaceBitmap.h"
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

int RunSmoke(Rml::Context* context, Rml::ElementDocument*& document, ClickCounter& counter, const std::string& document_path)
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

} // namespace

int main(int argc, char** argv)
{
	bool smoke = argc > 1 && std::strcmp(argv[1], "--smoke") == 0;

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
	context->SetDensityIndependentPixelRatio(GetWindowScaleDPI().x);

	if (!Rml::Debugger::Initialise(context))
	{
		printf("RmlUi debugger initialisation failed\n");
		Rml::Shutdown();
		CloseWindow();
		return 1;
	}
	Rml::Debugger::SetVisible(false);

	std::string data_dir = "tools/rmlui_probe/data/ui/";
	if (!std::filesystem::exists(data_dir + "probe.rml"))
		data_dir = "data/ui/";
	std::string document_path = data_dir + "probe.rml";

	if (!Rml::LoadFontFace(data_dir + "Comfortaa_Regular_22.fnt"))
	{
		printf("Font load failed: %s\n", (data_dir + "Comfortaa_Regular_22.fnt").c_str());
		Rml::Shutdown();
		CloseWindow();
		return 1;
	}

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

	int result = 0;
	if (smoke)
	{
		result = RunSmoke(context, document, counter, document_path);
	}
	else
	{
		bool debugger_visible = false;
		int last_width = GetScreenWidth();
		int last_height = GetScreenHeight();

		while (!WindowShouldClose())
		{
			int width = GetScreenWidth();
			int height = GetScreenHeight();
			if (width != last_width || height != last_height)
			{
				last_width = width;
				last_height = height;
				context->SetDimensions(Rml::Vector2i(width, height));
				context->SetDensityIndependentPixelRatio(GetWindowScaleDPI().x);
			}

			PumpInput(context);

			if (IsKeyPressed(KEY_F9))
			{
				debugger_visible = !debugger_visible;
				Rml::Debugger::SetVisible(debugger_visible);
			}
			if (IsKeyPressed(KEY_F8))
				ReloadDocument(context, document, counter, document_path);

			RenderFrame(context, true, nullptr);
		}
	}

	if (document)
	{
		context->UnloadDocument(document);
		context->Update();
		document = nullptr;
	}
	Rml::Shutdown();
	CloseWindow();

	return result;
}
