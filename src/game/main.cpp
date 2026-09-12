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
	// deselects, Space halts selected units.
	InputManager input;
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

	while (!WindowShouldClose())
	{
		// M2 Goal 6: single input pump (WASD + shortcuts + mouse snapshot).
		input.Update(camera, kCameraSpeed, GetFrameTime());

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
		for (int i = 0; i < static_cast<int>(nodes.Count()); ++i)
		{
		}
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
		registry.Each<Unit>([&](Entity, Unit &unit) {
			const Rectangle body = { unit.position.x + 16.0f, unit.position.y + 16.0f, 32.0f, 32.0f };
			const Vector2 center = { body.x + 16.0f, body.y + 16.0f };
			DrawRectangleRec(body, unit.state == UnitState::Attacking ? ORANGE : BLUE);
			if (unit.isSelected)
			{
				DrawRectangleLinesEx(body, 3.0f, RED);
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

		// M5 HUD preview (proper raygui panels in M6): stockpiles + queue.
		DrawText(TextFormat("Iron: %ld  Oil: %ld", resources.iron, resources.oil), 620, 20, 20,
		         DARKGRAY);
		if (!queue.Empty())
		{
			DrawText("Producing...", 620, 48, 16, GRAY);
			DrawRectangle(620, 68, 150, 12, LIGHTGRAY);
			DrawRectangle(620, 68, static_cast<int>(150.0f * queue.HeadProgress()), 12, DARKGREEN);
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
		EndDrawing();
	}

	CloseWindow();
	return 0;
}
