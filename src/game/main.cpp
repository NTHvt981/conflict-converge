// main.cpp - RTS Game Entry Point
// This is a basic template using raylib for an RTS game

#include "raylib.h"
#include "raygui.h" // M1 Goal 4: raygui UI framework (impl TU: src/thirdparty/raygui_impl.c)
#include "GameCamera.h" // M2 Goal 3: manual WASD panning camera
#include "Registry.h"  // M2 Goal 4: unit registry (selection + orders)
#include "Selection.h" // M2 Goal 4: mouse selection helpers
#include "InputManager.h" // M2 Goal 6: per-frame input pump (WASD, mouse, shortcuts)
#include "TileMap.h"   // M2 Goal 1: passable/blocked tile grid
#include "Unit.h"      // M2 Goal 2/4: snapped units with move orders
#include "UnitFactory.h" // M3 Goal 6: cost-validated demo spawns + death sweep
#include "Pathfinder.h" // M3 Goal 3: right-click orders route around blocks
#include "Building.h" // M5 Goal 2: demo base/placement on the tile grid
#include "Nodes.h" // M5 Goal 3: demo resource nodes + harvester
#include "Production.h" // M5 Goal 6: demo factory production queue
#include "Minimap.h" // M6 Goal 1: unit-position minimap (periodic refresh)
#include "Hud.h" // M6 Goal 2: raygui resource + selection panels
#include "Menu.h" // M6 Goal 3: pause / outcome / settings menu flow
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
	// M6 Goal 3: menu flow (pause/outcome/settings); camera speed is a
	// live setting, not a constant, so the settings slider can tune it.
	MenuFlow menu;

	// M6 Goal 1: minimap texture (bottom-right, 4:3 like the 20x15 map).
	Minimap minimap;
	minimap.Init({ static_cast<float>(screenWidth) - 170.0f, static_cast<float>(screenHeight) - 130.0f,
	               160.0f, 120.0f });
	// Unit speed now comes from M3 base stats (Unit::speed, ApplyBaseStats).

	// M3 Goal 6 demo world: units spawn through the factory (costs deducted
	// from starting funds, UnitSpawned announced) on the tile map. Placeholder
	// art is colored rectangles (see M2 blockings); sprites arrive later.
	Registry registry;
	ResourceSystem resources;
	resources.AddIron(1000);
	resources.AddOil(500);
	EventDispatcher events;
	UnitFactory factory(registry, resources, events);
	TileMap map(20, 15);
	map.Set({ 6, 3 }, TerrainType::Water);
	map.Set({ 7, 3 }, TerrainType::Water);
	map.Set({ 6, 4 }, TerrainType::Water);
	auto spawnDemo = [&](UnitType type, int tileX, int tileY, int team) {
		factory.Spawn(type, team, cc::ToRaylib(cc::TileToWorld(tileX, tileY)));
	};
	spawnDemo(UnitType::Infantry, 2, 2, 0);
	spawnDemo(UnitType::LightTank, 4, 2, 0);
	spawnDemo(UnitType::Artillery, 3, 5, 0);
	spawnDemo(UnitType::LightTank, 6, 2, 1); // M3 Goal 5: hostile, in sight of the infantry

	// M5 demo economy: home base + factory + depot, two field nodes with a
	// harvester Engineer parked on the iron, and a factory queue building
	// reinforcements at the rally point. Costs come out of starting funds.
	PlaceBuilding(registry, map, BuildingType::Base, 0, 1, 10);
	PlaceBuilding(registry, map, BuildingType::ResourceDepot, 0, 5, 10);
	PlaceBuilding(registry, map, BuildingType::Factory, 0, 10, 10);
	ResourceNodes nodes;
	nodes.SpawnNode(map, ResourceKind::Iron, { 15, 3 }, 200.0f, 10.0f);
	nodes.SpawnNode(map, ResourceKind::Oil, { 15, 12 }, 150.0f, 10.0f);
	spawnDemo(UnitType::Engineer, 15, 3, 0); // harvester: idles on the iron node
	ProductionQueue queue;
	queue.Enqueue(resources, UnitType::Infantry);
	queue.Enqueue(resources, UnitType::LightTank);
	const Vector2 rallyPos = cc::ToRaylib(cc::TileToWorld(13, 11));

	// M2 Goal 5 shortcuts, pumped by the M2 Goal 6 InputManager: Esc
	// deselects, Space halts selected units, P pauses (M6 Goal 3),
	// F1 toggles the shortcut overlay (M6 Goal 4).
	InputManager input;
	bool showHints = true; // M6 Goal 4: F1 toggles the shortcut overlay
	input.shortcuts.Bind(KEY_F1, [&] { showHints = !showHints; });
	input.shortcuts.Bind(KEY_P, [&] { menu.TogglePause(); });
	input.shortcuts.Bind(KEY_ESCAPE, [&] { DeselectAll(registry); });
	input.shortcuts.Bind(KEY_SPACE, [&] {
		registry.Each<Unit>([&](Entity, Unit &unit) {
			if (unit.isSelected)
			{
				unit.hasMoveOrder = false;
				unit.hasPath = false;
				unit.path.clear();
				unit.pathNext = 0;
				unit.target = kInvalidEntity; // M3 Goal 5: halt drops combat too
				unit.phase = AttackPhase::Ready; // M4 Goal 3: halt cancels the telegraph
				unit.velocity = { 0.0f, 0.0f };
				unit.state = UnitState::Idle;
				SnapUnitToTile(unit);
			}
		});
	});

	// M1 Goal 4 proof-of-integration: a raygui button counting clicks.
	// Full HUD/minimap/menu UI lands in M6.
	int buttonClicks = 0;
	bool showInfoBox = true;

	while (!WindowShouldClose() && !menu.quitRequested)
	{
		// M2 Goal 6: single input pump (always runs: P must unpause too).
		// Camera speed is a live menu setting (M6 Goal 3).
		input.Update(camera, menu.settings.cameraSpeed, GetFrameTime());

		// M6 Goal 3: orders, AI, economy, and minimap only advance while
		// Playing; rendering below always runs so menus overlay a live frame.
		if (menu.state == MenuState::Playing)
		{

			// M2 Goal 4 mouse inputs: left-click selects, right-click orders.
			// M3 Goal 3: orders pathfind around water/buildings via IssuePathOrder.
			if (input.LeftPressed())
			{
				const Vector2 world = input.MouseWorld(camera);
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
			if (input.RightPressed())
			{
				const Entity selected = SelectedUnit(registry);
				if (Unit *ordered = registry.Get<Unit>(selected))
				{
					IssuePathOrder(*ordered, map, input.MouseWorld(camera));
				}
			}
			registry.Each<Unit>([&](Entity id, Unit &unit) {
				UpdateUnit(id, registry, map, GetFrameTime()); // M3 Goal 5: AI driver
			});
			// M3 Goal 6: collect the fallen, then destroy through the factory so
			// UnitDestroyed is announced (destroying inside Each would invalidate it).
			std::vector<Entity> dead;
			registry.Each<Unit>([&](Entity id, const Unit &unit) {
				if (unit.health <= 0.0f)
				{
					dead.push_back(id);
				}
			});
			for (Entity id : dead)
			{
				factory.DestroyUnit(id);
			}
			// M5 economy tick: base trickle, node respawn, harvest, production.
			const float dt = GetFrameTime();
			UpdateBaseIncome(registry, resources, dt, 0);
			nodes.Update(dt);
			nodes.GatherTick(registry, resources, dt);
			queue.Update(factory, 0, rallyPos, dt);
			// M6 Goal 1: periodic minimap refresh (terrain blocks + unit dots).
			if (minimap.PollRefresh(dt))
			{
				BeginTextureMode(minimap.target);
				ClearBackground(Color{ 20, 60, 20, 255 }); // dark grass base
				for (int y = 0; y < map.Height(); ++y)
				{
					for (int x = 0; x < map.Width(); ++x)
					{
						const TerrainType terrain = map.Get({ x, y });
						if (terrain == TerrainType::Grass)
						{
							continue;
						}
						const Vector2 corner = minimap.WorldToMinimap(cc::ToRaylib(cc::TileToWorld(x, y)),
						                                             map.Width(), map.Height());
						const Color tint = terrain == TerrainType::Water ? DARKBLUE : DARKGRAY;
						DrawRectangleV({ corner.x - minimap.screenRect.x, corner.y - minimap.screenRect.y },
						               { 8.0f, 8.0f }, tint);
					}
				}
				registry.Each<Unit>([&](Entity, const Unit &unit) {
					const Vector2 center = { unit.position.x + 32.0f, unit.position.y + 32.0f };
					const Vector2 dot =
						minimap.WorldToMinimap(center, map.Width(), map.Height());
					DrawRectangle(static_cast<int>(dot.x - minimap.screenRect.x) - 1,
					              static_cast<int>(dot.y - minimap.screenRect.y) - 1, 3, 3,
					              unit.teamID == 0 ? SKYBLUE : RED);
				});
				EndTextureMode();
			}
			// M6 Goal 3: decide terminal states from the living rosters.
			menu.ShowOutcome(TeamHasUnits(registry, 0), TeamHasUnits(registry, 1));
		}

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

		// M5: buildings as footprint rects with a type letter, nodes as
		// kind-colored discs with remaining amounts.
		registry.Each<Building>([&](Entity, const Building &building) {
			const cc::IVec2 size = Footprint(building.type);
			const Vector2 corner = cc::ToRaylib(cc::TileToWorld(building.tileX, building.tileY));
			const float w = static_cast<float>(size.x) * cc::TILE_SIZE;
			const float h = static_cast<float>(size.y) * cc::TILE_SIZE;
			const Color tint =
				building.type == BuildingType::Base ? DARKGRAY : building.type == BuildingType::Factory ? BROWN : GRAY;
			DrawRectangleV(corner, { w, h }, tint);
			const char *label =
				building.type == BuildingType::Base ? "B" : building.type == BuildingType::Factory ? "F" : "D";
			DrawText(label, static_cast<int>(corner.x) + 6, static_cast<int>(corner.y) + 4, 24, WHITE);
		});
		nodes.Each([&](const ResourceNode &node) {
			const Vector2 corner = cc::ToRaylib(cc::TileToWorld(node.tile.x, node.tile.y));
			const Vector2 center = { corner.x + cc::TILE_SIZE / 2.0f,
				                     corner.y + cc::TILE_SIZE / 2.0f };
			DrawCircleV(center, 14.0f, node.kind == ResourceKind::Iron ? GOLD : LIME);
			DrawText(TextFormat("%.0f", node.amount), static_cast<int>(center.x) - 12,
			         static_cast<int>(center.y) - 8, 12, DARKGRAY);
		});

		// Units as 32x32 placeholder rects: red-ringed when selected,
		// orange-bodied when Attacking with a tracer to the target (M3G5),
		// white-flashed with a damage number while hitFlashTime runs (M4G5).
		// Selected units also get a health bar (M6 Goal 4).
		registry.Each<Unit>([&](Entity, Unit &unit) {
			const Rectangle body = { unit.position.x + 16.0f, unit.position.y + 16.0f, 32.0f, 32.0f };
			const Vector2 center = { body.x + 16.0f, body.y + 16.0f };
			DrawRectangleRec(body, unit.state == UnitState::Attacking ? ORANGE : BLUE);
			if (unit.isSelected)
			{
				DrawRectangleLinesEx(body, 3.0f, RED);
				const float fraction = UnitHealthFraction(unit);
				DrawRectangle(static_cast<int>(body.x), static_cast<int>(body.y) - 8,
				              static_cast<int>(body.width), 5, Fade(RED, 0.6f));
				DrawRectangle(static_cast<int>(body.x), static_cast<int>(body.y) - 8,
				              static_cast<int>(body.width * fraction), 5, GREEN);
			}
			if (unit.hitFlashTime > 0.0f)
			{
				DrawRectangleRec(body, Fade(WHITE, 0.7f));
				DrawText(TextFormat("-%.0f", unit.lastDamageTaken), static_cast<int>(body.x),
				         static_cast<int>(body.y) - 18, 16, RED);
			}
			if (unit.state == UnitState::Attacking)
			{
				if (const Unit *target = registry.Get<Unit>(unit.target))
				{
					const Vector2 targetCenter = { target->position.x + 32.0f, target->position.y + 32.0f };
					DrawLineV(center, targetCenter, RED);
				}
			}
			if (unit.hasMoveOrder)
			{
				DrawCircleV(cc::ToRaylib(cc::ToGlm(unit.moveTarget) + cc::Vec2(32.0f, 32.0f)), 5.0f, GREEN);
			}
		});
		EndMode2D();

		// M6 Goal 1: minimap blit (texture is Y-flipped) + viewport box.
		// Hidden from the settings panel (M6 Goal 3).
		if (menu.settings.showMinimap)
		{
			DrawTextureRec(minimap.target.texture,
			               { 0.0f, 0.0f, minimap.screenRect.width, -minimap.screenRect.height },
			               { minimap.screenRect.x, minimap.screenRect.y }, WHITE);
			DrawRectangleLinesEx(
				minimap.ViewportRect(camera.view, screenWidth, screenHeight, map.Width(), map.Height()),
				1.0f, WHITE);
			DrawRectangleLinesEx(minimap.screenRect, 1.0f, DARKGRAY);
		}

		// M6 Goal 2: raygui HUD (proper panels replace the M5 text counters).
		DrawResourcePanel(resources);
		DrawSelectionPanel(registry);
		if (!queue.Empty())
		{
			DrawText("Producing...", 620, 48, 16, GRAY);
			DrawRectangle(620, 68, 150, 12, LIGHTGRAY);
			DrawRectangle(620, 68, static_cast<int>(150.0f * queue.HeadProgress()), 12, DARKGREEN);
		}

		// M6 Goal 4: shortcut overlay, bottom-left, toggled with F1.
		if (showHints)
		{
			const std::vector<std::string> hints = ShortcutHintLines();
			for (std::size_t i = 0; i < hints.size(); ++i)
			{
				DrawText(hints[i].c_str(), 8, 250 + static_cast<int>(i) * 18, 14, Fade(DARKGRAY, 0.8f));
			}
		}

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
		// M6 Goal 3: menu overlays sit on top of the frame.
		if (menu.state == MenuState::Paused)
		{
			DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.5f));
			if (GuiWindowBox(Rectangle{ 250, 90, 300, 270 }, "Paused"))
			{
				menu.state = MenuState::Playing;
			}
			if (GuiButton(Rectangle{ 270, 135, 260, 30 }, "Resume"))
			{
				menu.state = MenuState::Playing;
			}
			GuiLabel(Rectangle{ 270, 172, 260, 20 }, "Camera speed");
			GuiSlider(Rectangle{ 270, 195, 260, 20 }, "100", "800", &menu.settings.cameraSpeed,
			          100.0f, 800.0f);
			GuiCheckBox(Rectangle{ 270, 222, 20, 20 }, "Minimap", &menu.settings.showMinimap);
			if (GuiButton(Rectangle{ 270, 315, 260, 30 }, "Quit to desktop"))
			{
				menu.quitRequested = true;
			}
		}
		else if (menu.state == MenuState::GameOver || menu.state == MenuState::Victory)
		{
			DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.6f));
			const bool won = menu.state == MenuState::Victory;
			if (GuiWindowBox(Rectangle{ 250, 150, 300, 150 }, won ? "Victory!" : "Defeat"))
			{
				menu.quitRequested = true;
			}
			GuiLabel(Rectangle{ 270, 195, 260, 20 },
			         won ? "Enemy force destroyed." : "Your force was destroyed.");
			if (GuiButton(Rectangle{ 270, 250, 260, 30 }, "Quit to desktop"))
			{
				menu.quitRequested = true;
			}
		}
		EndDrawing();
	}

	minimap.Unload();
	CloseWindow();
	return 0;
}
