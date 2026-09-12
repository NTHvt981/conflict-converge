// main.cpp - RTS Game Entry Point
// This is a basic template using raylib for an RTS game

#include "raylib.h"
#include "raygui.h" // M1 Goal 4: raygui UI framework (impl TU: src/thirdparty/raygui_impl.c)
#include "GameCamera.h" // M2 Goal 3: manual WASD panning camera
#include <iostream>
#include <vector>
#include <format>

int main(void)
{
	const int screenWidth = 800;
	const int screenHeight = 450;

	InitWindow(screenWidth, screenHeight, "raylib basic window");
	SetTargetFPS(60);

	GameCamera camera;
	camera.view.offset = { screenWidth / 2.0f, screenHeight / 2.0f };
	camera.view.rotation = 0.0f;
	camera.view.zoom = 1.0f;
	constexpr float kCameraSpeed = 400.0f; // pixels per second

	// M1 Goal 4 proof-of-integration: a raygui button counting clicks.
	// Full HUD/minimap/menu UI lands in M6.
	int buttonClicks = 0;
	bool showInfoBox = true;

	while (!WindowShouldClose())
	{
		camera.UpdateWASD(kCameraSpeed, GetFrameTime());

		BeginDrawing();
		ClearBackground(RAYWHITE);

		BeginMode2D(camera.view);
		DrawText("Welcome to raylib!", 200, 120, 40, DARKGRAY);
		DrawText("WASD pans the camera", 200, 170, 20, GRAY);
		EndMode2D();

		if (GuiButton(Rectangle{ 20, 20, 140, 30 }, "Click me"))
		{
			buttonClicks++;
		}
		DrawText(TextFormat("raygui clicks: %d", buttonClicks), 20, 60, 20, DARKGRAY);

		if (showInfoBox)
		{
			showInfoBox = !GuiWindowBox(Rectangle{ 20, 100, 260, 100 }, "raygui works");
			GuiLabel(Rectangle{ 40, 140, 220, 20 }, "UI framework integrated (M1).");
		}
		EndDrawing();
	}

	CloseWindow();
	return 0;
}
