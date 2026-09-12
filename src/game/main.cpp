// main.cpp - RTS Game Entry Point
// This is a basic template using raylib for an RTS game

#include "raylib.h"
#include "raygui.h" // M1 Goal 4: raygui UI framework (impl TU: src/thirdparty/raygui_impl.c)
#include "GameCamera.h" // M2 Goal 3: manual WASD panning camera
#include "Registry.h"  // M2 Goal 4: unit registry (selection + orders)
#include "Selection.h" // M2 Goal 4: mouse selection helpers
#include "TileMap.h"   // M2 Goal 1: passable/blocked tile grid
#include "Unit.h"      // M2 Goal 2/4: snapped units with move orders
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
	constexpr float kUnitSpeed = 200.0f; // pixels per second

	// M2 Goal 4 demo world: registry of units on a tile map. Placeholder
	// art is colored rectangles (see M2 blockings); sprites arrive later.
	Registry registry;
	TileMap map(20, 15);
	map.Set({ 6, 3 }, TerrainType::Water);
	map.Set({ 7, 3 }, TerrainType::Water);
	map.Set({ 6, 4 }, TerrainType::Water);
	auto spawnDemo = [&](UnitType type, int tileX, int tileY) {
		Unit unit;
		unit.type = type;
		unit.position = cc::ToRaylib(cc::TileToWorld(tileX, tileY));
		SnapUnitToTile(unit);
		registry.Add(registry.Create(), unit);
	};
	spawnDemo(UnitType::Infantry, 2, 2);
	spawnDemo(UnitType::LightTank, 4, 2);
	spawnDemo(UnitType::Artillery, 3, 5);

	// M1 Goal 4 proof-of-integration: a raygui button counting clicks.
	// Full HUD/minimap/menu UI lands in M6.
	int buttonClicks = 0;
	bool showInfoBox = true;

	while (!WindowShouldClose())
	{
		camera.UpdateWASD(kCameraSpeed, GetFrameTime());

		// M2 Goal 4 mouse inputs: left-click selects, right-click orders.
		if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
		{
			const Vector2 world = camera.ScreenToWorld(GetMousePosition());
			const Entity hit = PickUnitAt(registry, world);
			if (hit != kInvalidEntity)
			{
				SelectOnly(registry, hit);
			}
			else
			{
				DeselectAll(registry);
			}
		}
		if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
		{
			const Entity selected = SelectedUnit(registry);
			if (Unit *ordered = registry.Get<Unit>(selected))
			{
				IssueMoveOrder(*ordered, camera.ScreenToWorld(GetMousePosition()));
			}
		}
		registry.Each<Unit>([&](Entity, Unit &unit) {
			UpdateUnitMovement(unit, map, kUnitSpeed, GetFrameTime());
		});

		BeginDrawing();
		ClearBackground(RAYWHITE);

		BeginMode2D(camera.view);
		DrawText("Welcome to raylib!", 200, 120, 40, DARKGRAY);
		DrawText("WASD pans the camera", 200, 170, 20, GRAY);
		DrawText("Left-click selects, right-click orders", 200, 195, 20, GRAY);

		// Tile grid: water filled, grass outlined.
		for (int y = 0; y < map.Height(); ++y)
		{
			for (int x = 0; x < map.Width(); ++x)
			{
				const Vector2 corner = cc::ToRaylib(cc::TileToWorld(x, y));
				if (map.Get({ x, y }) == TerrainType::Water)
				{
					DrawRectangleV(corner, { cc::TILE_SIZE, cc::TILE_SIZE }, SKYBLUE);
				}
				DrawRectangleLinesEx({ corner.x, corner.y, cc::TILE_SIZE, cc::TILE_SIZE }, 1.0f, LIGHTGRAY);
			}
		}

		// Units as 32x32 placeholder rects, red-ringed when selected.
		registry.Each<Unit>([&](Entity, Unit &unit) {
			const Rectangle body = { unit.position.x + 16.0f, unit.position.y + 16.0f, 32.0f, 32.0f };
			DrawRectangleRec(body, BLUE);
			if (unit.isSelected)
			{
				DrawRectangleLinesEx(body, 3.0f, RED);
			}
			if (unit.hasMoveOrder)
			{
				DrawCircleV(cc::ToRaylib(cc::ToGlm(unit.moveTarget) + cc::Vec2(32.0f, 32.0f)), 5.0f, GREEN);
			}
		});
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
