# Module Map

On-demand reference for per-header responsibilities. `AGENTS.md` carries only the
compact module map; read this before touching an unfamiliar subsystem.

`src/game/public/<core|world|units|economy|app>/` headers + `src/game/private/<same>/`
sources per module. Entry is `src/game/main.cpp`; `Game` (core) owns state + loop. Only
`src/game/public` and `src/game/private` are on the include path (see `includedirs` in
`premake5.lua`), so includes stay subsystem-qualified (`#include "units/Unit.h"`) and moves
across subsystems need include edits. `Unit.h` fwd-declares `TileMap`; never include
`Pathfinder.h` from `Unit.h` — no cycles.

```
core:    MathUtils, CcAssert, Registry, Event, Subsystem
         Game, Simulation, PlayingInput, MenuScreens
world:   TileMap, Pathfinder, MapFile, FogOfWar
units:   Unit, UnitStats, Combat, UnitFactory, Targeting, Formation, AICommander,
          Extensions, UnitCommands
economy: Building, Nodes, ResourceSystem, Production
app:     GameCamera, InputManager, Selection, Shortcuts, Hotkeys, Minimap, Hud, Art,
         SpriteData, UnitConfig, Menu, Skirmish, SaveGame, Audio, Log, Cursor, Pings,
         Shake, DataRoot, RmlUiHost, RmlUiMenus, RmlUiHud, RmlRaylibRenderInterface,
         RmlRaylibSystemInterface, RmlRaylibFileInterface
```

## core

- `Game.h` - Owns match state + frame loop (`Init`/`Update`/`Shutdown`/`IsRunning`/`Run`);
  `main.cpp` is entry-only. Services live in scope containers declared first
  (`engine_` process lifetime, `world_` match lifetime, `player_` view state;
  construction order is load-bearing — init lists bind `engine_.Get<T>()` etc.).
  `MatchController` fans out `world_.ResetForMatch()` at match boundaries.
  `worldIs2v2` gates the ally/enemyAI2 ticks (never unit counts —
  shared teams wake parked commanders); `E2E*` seam is for tests/e2e only
- `Simulation.h` - Per-frame match tick extracted from `Game::Update` (no input/menus/
  rendering, so unit tests drive `Step` headlessly); `ResetForMatch`/`ResetEdgePolls`, team-0
  factory gate, replay recording
- `PlayingInput.h` - In-match input dispatch extracted from `Game`: drag-box select,
  area-build/repair, right-click orders, line formation, control groups, sticky gesture
  modes, armed HUD abilities. Runs only while Playing; advances no simulation
- `MenuScreens.h` - Map Editor branch only (raygui scratch canvas) + hotkey
  override apply/sync (`ApplyHotkeyOverrides`/`SyncHotkeySettings`); menus/HUD are
  RmlUi (`RmlUiMenus`/`RmlUiHud`). `MenuCallbacks` gives `Game` every world mutation.
  Needs window + GL, not headless-testable
- `Registry.h` - ECS-lite: `uint32_t` entities (0 never issued, free-list recycle), per-type
  pools, `Each<T>` iteration
- `Event.h` - `EventDispatcher`: Subscribe/Dispatch(in order)/Clear by EventType, base-`Event`
  + downcast payloads, M14 Q57 categories (game-state: MatchStarted/Paused/GameOver/Victory;
  UI: MenuAction/ProductionOrdered)
- `MathUtils.h` - `cc::Vec2`, raylib conversions, 64x64 tile helpers (top-left-corner
  convention)
- `CcAssert.h` - `CC_ASSERT`: live in Debug, `((void)0)` under NDEBUG
- `Subsystem.h` - UE5-inspired service scopes (no reflection): `Subsystem` base
  (`Init`/`Shutdown`/`ResetForMatch` no-op defaults) + `Subsystems` container
  (`Add`/`Get`/`TryGet`, keyed lookup for repeated types like `AICommander`);
  `Game` owns `engine_`/`world_`/`player_` scopes (see `Game.h`)

## world

- `TileMap.h` - Grid dims, `TerrainType` Grass/Water/Building/Forest/Rock (Forest passable,
  Rock blocks; new types append after Building so old saves still decode), `IsBlocked`
  (Water/Building/Rock + OOB = true)
- `Pathfinder.h` - 4-dir A* (`FindPath`, float g-cost via `TerrainCost`, uniform 1.0) +
  `IssuePathOrder` (M2 straight fallback when unreachable)
- `MapFile.h` - Text `.map` loader (`data/`): 5-line header, legend (Q79), markers,
  `ApplyMapData` (default node amounts), `NearestFreeTile` for spawns, M14 `ListMaps`
  (sorted setup entries, skips unparseable)
- `FogOfWar.h` - M9 per-team visibility (pure-radius circles, scout bonus, explored memory,
  save/load bytes)

## units

- `Unit.h` - Full component: stats, A* path fields, `UpdateUnit` (non-const map: demolish
  mutates), stances, attack-move/patrol/repair orders, hit-flash fields, `SeparateUnits`
  overlap avoidance. Fwd-declares `TileMap`; never include `Pathfinder.h` from here.
  Gimmick drivers: `ResolveCrush`, turret traverse + fire-gate in `EngageTarget`,
  player-only `Load`/`Unload` orders (`CanLoadTarget`/`BoardTransport`/`UnloadTransport`)
- `UnitCommands.h` - Selection-batch orders shared by hotkeys, HUD ability
  buttons, and armed right-clicks: stances, halt, auto-retreat/repair toggles,
  attack-move/patrol/repair/heal issue
- `UnitStats.h` - `BaseStats` table for 7 types + `ApplyBaseStats` (preserves
  position/team/selection)
- `Combat.h` - 3x3 damage matrix (`Effectiveness`), `ResolveAttack` (scales, restarts
  cooldown, stamps hit-flash), hitboxes, crush rule, M13 `ResolveBuildingAttack`;
  M3 `ResolveStrike`/`ResolveStrikeGround` dispatch (direct vs arcing launch) +
  `UpdateProjectiles` ballistics (dodgeable AoE, no friendly fire, never persisted)
- `UnitFactory.h` - Cost-validated `Spawn` / prepaid `SpawnPrepaid` / `DestroyUnit`
  (`UnitSpawned`/`UnitDestroyed` events)
- `Targeting.h` - Nearest-enemy/threat acquisition + range check (M9 fog gate, artillery
  exempt) + M13 `AcquireBuildingTarget`/`BuildingCenter`
- `Formation.h` - Row-major offsets + `IssueFormationMove`
- `AICommander.h` - M8 enemy commander: own fair-rules economy, build order, threshold waves,
  scouting, Easy/Medium/Hard; M14 `Reset` (team + difficulty + homes; re-arm for a new match
  or bare for the load path); 2v2 overflow via `allyAI` (team 0) + `enemyAI2` (team 1),
  shared-team economy credit

## economy

- `Building.h` - Base/Depot/Factory footprints, tile-grid placement (marks
  `TerrainType::Building`), demolish, base income, M13 structure HP (`BuildingMaxHealth`)
- `Production.h` - Factory queue: upfront charge, FIFO, prepaid spawn at rally (main + AI
  gate `Update` on a live Factory)
- `ResourceSystem.h` - Iron/oil ledger: `Add`/`TrySpend` + `TickIncome` (fractional carry,
  no cap)
- `Nodes.h` - Resource nodes: spawn/gather/deplete/respawn, Engineer harvesting, `Each`
  iteration

## app

- `GameCamera.h` - WASD camera over raylib `Camera2D` (named GameCamera: raylib owns
  `Camera`); M13 `AdjustZoom` clamped [`kMinZoom`, `kMaxZoom`], world-based zoom-out floor
  (`MinZoomForWorld`/`ClampZoomToWorld`: map always fills the screen), `ClampToMap` world
  bounds
- `InputManager.h` - Per-frame pump: WASD + shortcuts + mouse snapshot incl. wheel/shift/hold
  (injectable `Snapshot` for tests)
- `Shortcuts.h` - Key→callback registry + Shift chords (`Fire`/`FireChord` are the
  headless-testable paths)
- `Hotkeys.h` - Fully remappable Tier-1 hotkeys: `kHotkeyDefs` is the single source for
  action ids/labels/default keys; `HotkeyMap` holds overrides + settings persistence
- `Selection.h` - Pick/select/deselect over Registry units + `NormalizeRect`/`SelectInRect`
  drag-box (center-hit, Shift extends)
- `Minimap.h` - Square top-right render-texture minimap sized by screen
  height, periodic refresh, aspect-preserved world transform with black-bar
  letterbox, viewport box, M13 inverse (`MinimapToWorld`/`Contains`)
- `Hud.h` - Pure text builders (`UnitTypeName`, `FormatResources`, `SelectionSummary`,
  `ShortcutHintLines`, production category/tab orders, `AbilitiesForSelection`
  panel data); the RmlUi HUD owns the panels
- `Art.h` - M12 sprites + particles (`data/sprites/` via `tools/gen_sprites.py`); `Init(false)`
  is the headless rectangle-fallback path
- `SpriteData.h` - Sprite-atlas data (JSON in `data/configs/`: textures/sprites/animations
  merged at load); `LoadSpriteSheet`/`ParseSpriteSheetJson` + name/id lookups
- `UnitConfig.h` - Unit-config editor data layer: one `UnitConfig` per
  `data/configs/<type>.json`, enum↔name parsing, load/save/enumerate. Pure logic, no raylib.
  M1 runtime authority: optional `abilities` block (turret/arcing/crush/transport/sniper,
  unknown keys reject) + `cost`; `ActiveUnitConfig` catalog loaded at match start with
  per-type compiled fallback; `CostOf` reads the catalog
- `Extensions.h` - M2 gimmick components in `Registry` pools: `Turret`/`Cargo`/`EmbarkedOn`
  (+ `Projectile` shells, `IsEmbarked`/`IsFleshUnit` gates). Header-only
- `Menu.h` - M14 boot-to-menu flow: MainMenu/SkirmishSetup/Settings/LoadGame +
  Playing/Paused/GameOver/Victory, injected-map setup (CanStart gate), `data/settings.cfg`
  persistence (Q86); pause overlay shares MenuSettings, quit-to-menu teardown
- `Skirmish.h` - M14 match build/teardown over `Game`'s boot-level objects: `SpotsForMap`
  markers, `BuildSkirmish` (funds, map, both bases, units, queue, AI Reset+SetupBase,
  camera/rally aim), `ResetSkirmish` (empty shell; loader rebuilds over it); 2v2 maps
  (twin_falls_2v2: two `1`+`2` markers) auto-arm allyAI/enemyAI2, `data/` postbuild-copied
  beside the exe
- `SaveGame.h` - Versioned binary save/load (`CCB2` magic, v2, cereal-binary via
  `SaveWire.h`): `WorldState` bundle, two-phase load, target-index remap, M13 named slots
  (`SaveSlotPath`)
- `Audio.h` - M11 synth-asset SFX + looped music (`data/audio/*.wav` via
  `tools/gen_audio.py`); `Init(false)` is the headless no-op path
- `Log.h` - Fatal logging over raylib `TraceLog`: `Init` tees console + file
  (`data/logs/conflict-converge.log`, wired in `Game::Init`/`Shutdown`), `Fatal` exits;
  headless-tested via `tests/unit-tests/log_tests.cpp`
- `Cursor.h` - Context-sensitive cursor intent (`PredictCursorIntent`): what a right-click
  would do at `worldPos` (Move/Attack/Repair/Heal/Load; unload is a self-click,
  predicted Move). Pure prediction, never mutates
- `Pings.h` - Attack/event pings: short-lived world-space markers for minimap blips +
  camera jump (`Raise`/`Update`/`Latest`); pure logic, headless-testable;
  `ResetForMatch` clears pings + retrigger floors on match boundaries
- `Shake.h` - Screen-shake trauma math (pure inline: add/decay/magnitude); `Game` renders
  through a shaken camera copy
- `DataRoot.h` - Launch hardening: `PickDataRoot` picks the chdir target so `data/` resolves
  from a foreign CWD

## entry / thirdparty / tests

- `src/game/main.cpp` - Entry only (`Game game; return game.Run();`); match boot, sim, and
  render all live in `Game` (core)
- `src/thirdparty/raygui_impl.c` - `RAYGUI_IMPLEMENTATION` TU for the header-only raygui lib
- `tests/` - `unit-tests/` (`test_harness.h` CHECK macro, `*_tests.cpp`, runner in
  `tests/main.cpp`) + `integration-tests/` (headless battle/economy/save-load, 200-unit perf
  budget) + `tests/e2e/` (tier-1 hidden-window Game-loop harness, own
  `conflict-converge-e2e` premake project + main; `removefiles` keeps it out of the headless
  runner). Test project compiles `src/game/private/**.cpp` in; game `main.cpp` excluded (own
  main)

## Repo / index notes

- Semantic code index (`open-codebase-index` plugin, `.opencode/codebase-index.json`,
  embeddinggemma): `docs/` is excluded — its prose outranked code on concept queries
  (previously `plans/`, same reason). NOTE the file is strict JSON (no comments) and
  `exclude` replaces the plugin's built-in patterns, so they are repeated inline; re-run
  `cbi index --force` after changing it.
