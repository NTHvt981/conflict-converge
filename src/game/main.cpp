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
#include "SaveGame.h" // M7 Goal 1: F5 quicksave / F9 quickload
#include "Minimap.h" // M6 Goal 1: unit-position minimap (periodic refresh)
#include "Hud.h" // M6 Goal 2: raygui resource + selection panels
#include "Menu.h" // M6 Goal 3: pause / outcome / settings menu flow
#include "FogOfWar.h" // M9: per-team visibility (shroud, targeting gate)
#include "Art.h" // M12: sprite renderer + particle pool (rectangle fallback)
#include "Audio.h" // M11: synthesized SFX + looped music (data/audio/*.wav)
#include "MapFile.h" // M10: demo loads Crossroads from data/
#include "AICommander.h" // M8: enemy commander (build order, waves, scouting)
#include <iostream>
#include <vector>
#include <format>
#include <cmath> // M12: muzzle direction normalization

int main(void)
{
	const int kInitialWidth = 1200;
	const int kInitialHeight = 675;

	SetConfigFlags(FLAG_WINDOW_RESIZABLE); // M13: user-resizable window
	InitWindow(kInitialWidth, kInitialHeight, "raylib basic window");
	SetWindowMinSize(800, 450); // HUD layout assumes at least this
	SetTargetFPS(60);

	// M11: audio device + asset load. The test binary never inits (headless).
	InitAudioDevice();
	Audio audio;
	audio.Init(IsAudioDeviceReady());
	// M12: sprite + particle renderer. Missing files fall back to the
	// rectangle placeholders the tests exercise headless.
	Art art;
	art.Init(true);
	// Poll state for edge-triggered sounds (placed/depleted counts, attack
	// rate limit, outcome transitions).
	int lastBuildingCount = 0;
	int lastDepletedCount = 0;
	float attackSfxTimer = 0.0f;
	MenuState lastOutcomeState = MenuState::Playing;
	bool hasFactory = false; // M13: recomputed per frame, gates queue + panel

	GameCamera camera;
	camera.view.offset = { kInitialWidth / 2.0f, kInitialHeight / 2.0f };
	camera.view.rotation = 0.0f;
	camera.view.zoom = 1.0f;
	// M6 Goal 3: menu flow (pause/outcome/settings); camera speed is a
	// live setting, not a constant, so the settings slider can tune it.
	MenuFlow menu;

	// M6 Goal 1: minimap texture (bottom-right, 4:3 like the 20x15 map).
	Minimap minimap;
	minimap.Init({ static_cast<float>(kInitialWidth) - 170.0f, static_cast<float>(kInitialHeight) - 130.0f,
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
	FogOfWar fog; // M9: recomputed every Playing frame, carried by WorldState
	ResourceNodes nodes;
	// M10: demo loads Crossroads; bases, units, and the harvester follow its
	// markers. A missing file falls back to the legacy hardcoded layout.
	MapData demoMap;
	cc::IVec2 playerHome{ 2, 2 };
	cc::IVec2 aiHome{ 16, 9 };
	cc::IVec2 harvestTile{ 15, 3 };
	if (ParseMapFile("data/crossroads.map", demoMap))
	{
		ApplyMapData(demoMap, map, nodes);
		if (!demoMap.playerSpawns.empty())
		{
			playerHome = demoMap.playerSpawns[0];
		}
		if (!demoMap.aiSpawns.empty())
		{
			aiHome = demoMap.aiSpawns[0];
		}
		for (const MapNodeSpawn &spawn : demoMap.nodes)
		{
			if (spawn.kind == ResourceKind::Iron)
			{
				harvestTile = spawn.tile;
				break;
			}
		}
	}
	else
	{
		map.Set({ 6, 3 }, TerrainType::Water);
		map.Set({ 7, 3 }, TerrainType::Water);
		map.Set({ 6, 4 }, TerrainType::Water);
		nodes.SpawnNode(map, ResourceKind::Iron, { 15, 3 }, 200.0f, 10.0f);
		nodes.SpawnNode(map, ResourceKind::Oil, { 15, 12 }, 150.0f, 10.0f);
	}
	fog.Resize(map.Width(), map.Height());
	auto spawnDemo = [&](UnitType type, int tileX, int tileY, int team) {
		factory.Spawn(type, team, cc::ToRaylib(cc::TileToWorld(tileX, tileY)));
	};
	spawnDemo(UnitType::Infantry, playerHome.x, playerHome.y, 0);
	spawnDemo(UnitType::LightTank, playerHome.x + 2, playerHome.y, 0);
	spawnDemo(UnitType::Artillery, playerHome.x + 1, playerHome.y + 3, 0);

	// M5 demo economy: home base + factory + depot around the player marker,
	// a harvester Engineer parked on the first iron node, and a factory queue
	// building reinforcements at the rally point. Costs come out of funds.
	auto placeDemoBase = [&](int team, cc::IVec2 anchor) {
		PlaceBuilding(registry, map, BuildingType::Base, team, anchor.x, anchor.y);
		const cc::IVec2 depotSpots[] = { { 2, 0 }, { 0, 2 }, { -1, 0 } };
		for (const cc::IVec2 &spot : depotSpots)
		{
			if (PlaceBuilding(registry, map, BuildingType::ResourceDepot, team, anchor.x + spot.x,
			                  anchor.y + spot.y) != kInvalidEntity)
			{
				break;
			}
		}
		const cc::IVec2 factorySpots[] = { { 0, 2 }, { 3, 0 }, { -2, 2 } };
		for (const cc::IVec2 &spot : factorySpots)
		{
			if (PlaceBuilding(registry, map, BuildingType::Factory, team, anchor.x + spot.x,
			                  anchor.y + spot.y) != kInvalidEntity)
			{
				break;
			}
		}
	};
	placeDemoBase(0, playerHome);
	spawnDemo(UnitType::Engineer, harvestTile.x, harvestTile.y, 0); // harvester on iron
	ProductionQueue queue;
	queue.Enqueue(resources, UnitType::Infantry);
	queue.Enqueue(resources, UnitType::LightTank);
	// M13: rally point moves by click (R toggles rally mode below).
	Vector2 rallyPos = cc::ToRaylib(cc::TileToWorld(playerHome.x + 4, playerHome.y));
	bool settingRally = false;

	// M8: enemy commander owns team 1 under fair rules (own funds, own
	// buildings, same order/spend APIs). Its starting guard keeps team 1
	// fielded from frame one so the outcome check never fires instantly.
	AICommander ai(registry, map, nodes, events, 1, AIDifficulty::Medium, aiHome, playerHome);
	ai.SetupBase();

	// M11: production/spawn confirmations. Subscribed after setup so the
	// boot-time spawns don't chatter.
	events.Subscribe(EventType::UnitSpawned, [&](const Event &) { audio.Play(SfxId::Confirm); });

	// M13: shared snapshot for the save-slot bindings below.
	WorldState worldState{ &registry, &resources, &map, &camera, &nodes, &fog };

	// M2 Goal 5 shortcuts, pumped by the M2 Goal 6 InputManager: Esc
	// deselects, Space halts selected units, P pauses (M6 Goal 3),
	// F1 toggles the shortcut overlay (M6 Goal 4).
	InputManager input;
	bool showHints = true; // M6 Goal 4: F1 toggles the shortcut overlay
	input.shortcuts.Bind(KEY_F1, [&] { showHints = !showHints; });
	input.shortcuts.Bind(KEY_P, [&] { menu.TogglePause(); });
	input.shortcuts.Bind(KEY_F5, [&] {
		SaveWorld({ &registry, &resources, &map, &camera, &nodes, &fog }, "data/quicksave.ccpb");
	});
	input.shortcuts.Bind(KEY_F9, [&] {
		LoadWorld({ &registry, &resources, &map, &camera, &nodes, &fog }, "data/quicksave.ccpb");
	});
	// M13: named save slots (F6-8 store, Shift+F6-8 recall).
	input.shortcuts.Bind(KEY_F6, [&] { SaveWorld(worldState, SaveSlotPath(1)); });
	input.shortcuts.Bind(KEY_F7, [&] { SaveWorld(worldState, SaveSlotPath(2)); });
	input.shortcuts.Bind(KEY_F8, [&] { SaveWorld(worldState, SaveSlotPath(3)); });
	input.shortcuts.BindChord(KEY_F6, [&] { LoadWorld(worldState, SaveSlotPath(1)); });
	input.shortcuts.BindChord(KEY_F7, [&] { LoadWorld(worldState, SaveSlotPath(2)); });
	input.shortcuts.BindChord(KEY_F8, [&] { LoadWorld(worldState, SaveSlotPath(3)); });
	// M13: order keys act on the current selection. A attack-moves to the
	// cursor, H/G switch stances, V patrols cursor-and-back, R toggles
	// rally-point placement.
	input.shortcuts.Bind(KEY_A, [&] {
		const Entity selected = SelectedUnit(registry);
		if (Unit *ordered = registry.Get<Unit>(selected))
		{
			IssueAttackMoveOrder(*ordered, map, input.MouseWorld(camera));
			audio.Play(SfxId::Confirm);
		}
	});
	input.shortcuts.Bind(KEY_H, [&] {
		const Entity selected = SelectedUnit(registry);
		if (Unit *unit = registry.Get<Unit>(selected))
		{
			SetStance(*unit, Stance::Hold);
		}
	});
	input.shortcuts.Bind(KEY_G, [&] {
		const Entity selected = SelectedUnit(registry);
		if (Unit *unit = registry.Get<Unit>(selected))
		{
			SetStance(*unit, Stance::Guard);
		}
	});
	input.shortcuts.Bind(KEY_V, [&] {
		const Entity selected = SelectedUnit(registry);
		if (Unit *ordered = registry.Get<Unit>(selected))
		{
			IssuePatrolOrder(*ordered, map, ordered->position, input.MouseWorld(camera));
			audio.Play(SfxId::Confirm);
		}
	});
	input.shortcuts.Bind(KEY_R, [&] { settingRally = !settingRally; });
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
				unit.attackMove = false; // M13: halt drops attack-move + repair too
				unit.hasRepairOrder = false;
				unit.repairTarget = kInvalidEntity;
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
		// M13: scroll-wheel zoom (clamped in GameCamera) runs even paused.
		camera.AdjustZoom(input.WheelDelta());
		// M13: resizable window — refresh live dims, keep the camera centered
		// and the minimap docked bottom-right (texture size is fixed).
		const int screenWidth = GetScreenWidth();
		const int screenHeight = GetScreenHeight();
		camera.view.offset = { screenWidth / 2.0f, screenHeight / 2.0f };
		minimap.screenRect.x = static_cast<float>(screenWidth) - minimap.screenRect.width - 10.0f;
		minimap.screenRect.y = static_cast<float>(screenHeight) - minimap.screenRect.height - 10.0f;

		// M6 Goal 3: orders, AI, economy, and minimap only advance while
		// Playing; rendering below always runs so menus overlay a live frame.
		if (menu.state == MenuState::Playing)
		{

			// M2 Goal 4 mouse inputs: left-click selects, right-click orders.
			// M3 Goal 3: orders pathfind around water/buildings via IssuePathOrder.
			// M13 routes minimap clicks to the camera and rally-mode clicks
			// to the factory rally point before unit selection.
			if (input.LeftPressed())
			{
				if (minimap.Contains(input.MouseScreen()))
				{
					camera.view.target =
						minimap.MinimapToWorld(input.MouseScreen(), map.Width(), map.Height());
				}
				else if (settingRally)
				{
					rallyPos = input.MouseWorld(camera);
					settingRally = false;
				}
				else
				{
					const Vector2 world = input.MouseWorld(camera);
					const Entity hit = PickUnitAt(registry, world);
					if (hit != kInvalidEntity)
					{
						SelectOnly(registry, hit);
						audio.Play(SfxId::Select); // M11: selection blip
					}
					else
					{
						DeselectAll(registry);
					}
				}
			}
			if (input.RightPressed())
			{
				const Entity selected = SelectedUnit(registry);
				if (Unit *ordered = registry.Get<Unit>(selected))
				{
					IssuePathOrder(*ordered, map, input.MouseWorld(camera));
					audio.Play(SfxId::Confirm); // M11: order acknowledged
				}
			}
			// M9: rebuild visibility from current positions before anyone acquires.
			fog.Recompute(registry);
			registry.Each<Unit>([&](Entity id, Unit &unit) {
				UpdateUnit(id, registry, map, GetFrameTime(), &fog); // M3G5 driver (+M9 fog gate)
			});
			// M3 Goal 6: collect the fallen, then destroy through the factory so
			// UnitDestroyed is announced (destroying inside Each would invalidate it).
			// dt is shared by the M11 polls below and the economy tick further down.
			const float dt = GetFrameTime();
			std::vector<Entity> dead;
			registry.Each<Unit>([&](Entity id, const Unit &unit) {
				if (unit.health <= 0.0f)
				{
					dead.push_back(id);
				}
			});
			for (Entity id : dead)
			{
				if (const Unit *corpse = registry.Get<Unit>(id))
				{
					// M12: death burst at the corpse before teardown.
					art.ParticlesPool().SpawnBurst(
						{ corpse->position.x + 32.0f, corpse->position.y + 32.0f }, ORANGE, 24,
						120.0f, 0.6f);
				}
				factory.DestroyUnit(id);
			}
			// M11: edge-triggered battle sounds (one explosion per wipe, not per corpse).
			if (!dead.empty())
			{
				audio.Play(SfxId::Explosion);
			}
			// M12: impact sparks on fresh hits + muzzle sparks while telegraphing.
			registry.Each<Unit>([&](Entity, const Unit &unit) {
				const Vector2 center = { unit.position.x + 32.0f, unit.position.y + 32.0f };
				if (unit.hitFlashTime > 0.20f)
				{
					art.ParticlesPool().SpawnBurst(center, YELLOW, 6, 90.0f, 0.25f);
				}
				if (unit.phase == AttackPhase::WindUp)
				{
					if (const Unit *target = registry.Get<Unit>(unit.target))
					{
						const Vector2 dir = { target->position.x - unit.position.x,
							                  target->position.y - unit.position.y };
						const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
						if (len > 1.0f)
						{
							art.ParticlesPool().SpawnBurst(
								{ center.x + dir.x / len * 20.0f, center.y + dir.y / len * 20.0f },
								ORANGE, 2, 40.0f, 0.15f);
						}
					}
				}
			});
			art.ParticlesPool().Update(dt);
			attackSfxTimer -= dt;
			bool windingUp = false;
			registry.Each<Unit>([&](Entity, const Unit &unit) {
				if (unit.phase == AttackPhase::WindUp)
				{
					windingUp = true;
				}
			});
			if (windingUp && attackSfxTimer <= 0.0f)
			{
				audio.Play(SfxId::Attack);
				attackSfxTimer = 0.12f;
			}
			int buildingCount = 0;
			registry.Each<Building>([&](Entity, const Building &building) {
				if (building.state == BuildingState::Operational)
				{
					++buildingCount;
				}
			});
			if (buildingCount > lastBuildingCount)
			{
				audio.Play(SfxId::Place);
			}
			lastBuildingCount = buildingCount;
			int depletedCount = 0;
			nodes.Each([&](const ResourceNode &node) {
				if (node.IsDepleted())
				{
					++depletedCount;
				}
			});
			if (depletedCount > lastDepletedCount)
			{
				audio.Play(SfxId::Deplete);
			}
			lastDepletedCount = depletedCount;
			// M5 economy tick: base trickle, node respawn, harvest, production.
			// (dt is declared up at the death sweep so the M11 polls above share it.)
			UpdateBaseIncome(registry, resources, dt, 0);
			nodes.Update(dt);
			nodes.GatherTick(registry, resources, dt, 0); // team 0 crew only (AI gathers its own)
			// M13: production dies with the structure — compute here for the
			// queue gate, reuse for the factory panel below.
			hasFactory = false;
			registry.Each<Building>([&](Entity, const Building &building) {
				if (building.teamID == 0 && building.type == BuildingType::Factory &&
				    building.state == BuildingState::Operational)
				{
					hasFactory = true;
				}
			});
			if (hasFactory)
			{
				queue.Update(factory, 0, rallyPos, dt);
			}
		ai.Update(dt); // M8: enemy build order, waves, scouting, retreat
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
						const Color tint = terrain == TerrainType::Water    ? DARKBLUE
						                     : terrain == TerrainType::Forest ? Color{ 20, 90, 20, 255 }
						                     : terrain == TerrainType::Rock   ? GRAY
						                                                      : DARKGRAY;
						DrawRectangleV({ corner.x - minimap.screenRect.x, corner.y - minimap.screenRect.y },
						               { 8.0f, 8.0f }, tint);
					}
				}
				registry.Each<Unit>([&](Entity, const Unit &unit) {
					// M9: enemies only appear when a team-0 unit sees their tile.
					if (unit.teamID != 0 &&
					    !fog.IsVisible(0, cc::WorldToTile(cc::ToGlm(unit.position))))
					{
						return;
					}
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
			// M11: fanfare on the transition frame only.
			if (menu.state != lastOutcomeState)
			{
				if (menu.state == MenuState::Victory)
				{
					audio.Play(SfxId::Victory);
				}
				else if (menu.state == MenuState::GameOver)
				{
					audio.Play(SfxId::Defeat);
				}
				lastOutcomeState = menu.state;
			}
		}

		// M11: volumes follow the pause-menu sliders live; the loop streams on.
		audio.ApplySettings(menu.settings.masterVolume, menu.settings.musicVolume,
		                    menu.settings.sfxVolume, menu.settings.mute);
		audio.UpdateMusic();

		BeginDrawing();
		ClearBackground(RAYWHITE);

		BeginMode2D(camera.view);

		// Tile grid: water/forest/rock filled, grass outlined.
		for (int y = 0; y < map.Height(); ++y)
		{
			for (int x = 0; x < map.Width(); ++x)
			{
				const Vector2 corner = cc::ToRaylib(cc::TileToWorld(x, y));
				const TerrainType terrain = map.Get({ x, y });
				if (terrain == TerrainType::Water)
				{
					DrawRectangleV(corner, { cc::TILE_SIZE, cc::TILE_SIZE }, SKYBLUE);
				}
				else if (terrain == TerrainType::Forest)
				{
					DrawRectangleV(corner, { cc::TILE_SIZE, cc::TILE_SIZE }, DARKGREEN);
				}
				else if (terrain == TerrainType::Rock)
				{
					DrawRectangleV(corner, { cc::TILE_SIZE, cc::TILE_SIZE }, GRAY);
				}
				DrawRectangleLinesEx({ corner.x, corner.y, cc::TILE_SIZE, cc::TILE_SIZE }, 1.0f, LIGHTGRAY);
			}
		}

		// M5: buildings as footprint rects with a type letter, nodes as
		// kind-colored discs with remaining amounts.
		registry.Each<Building>([&](Entity, const Building &building) {
			// M9: enemy structures hide until a team-0 unit sees a footprint tile.
			if (building.teamID != 0 &&
			    !fog.IsVisible(0, cc::IVec2{ building.tileX, building.tileY }))
			{
				return;
			}
			const cc::IVec2 size = Footprint(building.type);
			const Vector2 corner = cc::ToRaylib(cc::TileToWorld(building.tileX, building.tileY));
			if (art.UseRectangles())
			{
				const float w = static_cast<float>(size.x) * cc::TILE_SIZE;
				const float h = static_cast<float>(size.y) * cc::TILE_SIZE;
				const Color tint =
					building.type == BuildingType::Base ? DARKGRAY : building.type == BuildingType::Factory ? BROWN : GRAY;
				DrawRectangleV(corner, { w, h }, tint);
				const char *label =
					building.type == BuildingType::Base ? "B" : building.type == BuildingType::Factory ? "F" : "D";
				DrawText(label, static_cast<int>(corner.x) + 6, static_cast<int>(corner.y) + 4, 24, WHITE);
			}
			else
			{
				art.DrawBuilding(building.type, building.teamID, building.tileX, building.tileY);
			}
		});
		nodes.Each([&](const ResourceNode &node) {
			// M9: static features join the frozen snapshot once explored.
			if (!fog.IsExplored(0, node.tile))
			{
				return;
			}
			const Vector2 corner = cc::ToRaylib(cc::TileToWorld(node.tile.x, node.tile.y));
			const Vector2 center = { corner.x + cc::TILE_SIZE / 2.0f,
				                     corner.y + cc::TILE_SIZE / 2.0f };
			if (art.UseRectangles())
			{
				DrawCircleV(center, 14.0f, node.kind == ResourceKind::Iron ? GOLD : LIME);
			}
			else
			{
				art.DrawNode(node.kind, center);
			}
			DrawText(TextFormat("%.0f", node.amount), static_cast<int>(center.x) - 12,
			         static_cast<int>(center.y) - 8, 12, DARKGRAY);
		});

		// Units as sprites (M12) or 32x32 placeholder rects (fallback/tests):
		// red-ringed when selected, tracer to the target when Attacking (M3G5),
		// white-flashed with a damage number while hitFlashTime runs (M4G5).
		// Selected units also get a health bar (M6 Goal 4).
		registry.Each<Unit>([&](Entity, Unit &unit) {
			// M9: enemies render only on tiles team 0 currently sees.
			if (unit.teamID != 0 &&
			    !fog.IsVisible(0, cc::WorldToTile(cc::ToGlm(unit.position))))
			{
				return;
			}
			const Rectangle body = { unit.position.x + 16.0f, unit.position.y + 16.0f, 32.0f, 32.0f };
			const Vector2 center = { body.x + 16.0f, body.y + 16.0f };
			if (art.UseRectangles())
			{
				DrawRectangleRec(body, unit.state == UnitState::Attacking ? ORANGE : BLUE);
			}
			else
			{
				art.DrawUnit(unit.type, unit.teamID, FrameForPhase(unit.phase), unit.position);
			}
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
		// M12: particles in world space, under the shroud so hidden battles
		// stay hidden.
		art.ParticlesPool().Draw();
		// M9 shroud, drawn over the world: unexplored tiles go opaque black,
		// explored-but-unseen tiles get a dim veil (frozen snapshot, Q76).
		for (int y = 0; y < map.Height(); ++y)
		{
			for (int x = 0; x < map.Width(); ++x)
			{
				const cc::IVec2 tile{ x, y };
				if (fog.IsVisible(0, tile))
				{
					continue;
				}
				const Vector2 corner = cc::ToRaylib(cc::TileToWorld(x, y));
				const Color veil =
					fog.IsExplored(0, tile) ? Fade(BLACK, 0.45f) : BLACK;
				DrawRectangleV(corner, { cc::TILE_SIZE, cc::TILE_SIZE }, veil);
			}
		}
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
		DrawResourcePanel(resources, &art);
		DrawSelectionPanel(registry);
		DrawSaveSlots();
		// M13: factory panel (build buttons, queue, cancel); rally hint
		// while placing the rally point. Recomputed here (not just the sim
		// gate above) so the panel stays correct while paused.
		registry.Each<Building>([&](Entity, const Building &building) {
			if (building.teamID == 0 && building.type == BuildingType::Factory &&
			    building.state == BuildingState::Operational)
			{
				hasFactory = true;
			}
		});
		DrawProductionPanel(resources, queue, hasFactory);
		if (settingRally)
		{
			DrawText("Rally: left-click to place (R cancels)", 250, 364, 16, DARKGREEN);
		}
		if (!queue.Empty())
		{
			DrawText("Producing...", 620, 48, 16, GRAY);
			DrawRectangle(620, 68, 150, 12, LIGHTGRAY);
			DrawRectangle(620, 68, static_cast<int>(150.0f * queue.HeadProgress()), 12, DARKGREEN);
		}

		// M7 Goal 3: live frame-rate readout (60 FPS target validation).
		DrawFPS(620, 88);
		// M8: enemy commander status (demo plays Medium).
		DrawText(TextFormat("Enemy: Medium  Waves: %d", ai.WavesLaunched()), 620, 108, 16, GRAY);

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
			if (GuiWindowBox(Rectangle{ 250, 40, 300, 360 }, "Paused"))
			{
				menu.state = MenuState::Playing;
			}
			if (GuiButton(Rectangle{ 270, 85, 260, 30 }, "Resume"))
			{
				menu.state = MenuState::Playing;
			}
			GuiLabel(Rectangle{ 270, 122, 260, 20 }, "Camera speed");
			GuiSlider(Rectangle{ 270, 145, 260, 20 }, "100", "800", &menu.settings.cameraSpeed,
			          100.0f, 800.0f);
			GuiCheckBox(Rectangle{ 270, 170, 20, 20 }, "Minimap", &menu.settings.showMinimap);
			// M11: volumes (0..1) + mute; file persistence arrives with M14.
			GuiLabel(Rectangle{ 270, 195, 260, 20 }, "Master volume");
			GuiSlider(Rectangle{ 270, 218, 260, 20 }, "0", "1", &menu.settings.masterVolume,
			          0.0f, 1.0f);
			GuiLabel(Rectangle{ 270, 243, 260, 20 }, "Music volume");
			GuiSlider(Rectangle{ 270, 266, 260, 20 }, "0", "1", &menu.settings.musicVolume,
			          0.0f, 1.0f);
			GuiLabel(Rectangle{ 270, 291, 260, 20 }, "SFX volume");
			GuiSlider(Rectangle{ 270, 314, 260, 20 }, "0", "1", &menu.settings.sfxVolume,
			          0.0f, 1.0f);
			GuiCheckBox(Rectangle{ 270, 340, 20, 20 }, "Mute", &menu.settings.mute);
			if (GuiButton(Rectangle{ 270, 365, 260, 30 }, "Quit to desktop"))
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
	art.Shutdown(); // M12: unload sprite textures
	audio.Shutdown(); // M11: unload sounds + music stream
	CloseAudioDevice();
	CloseWindow();
	return 0;
}
