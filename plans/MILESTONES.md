# Conflict Converge - Development Milestones

## Project Summary
- **Name**: Conflict Converge
- **Genre**: Real-Time Strategy (RTS)
- **Engine**: raylib with glm (math), raygui (UI)
- **Build System**: premake5 + msbuild
- **Art Style**: 2D Pixel Art (32x32 sprites)
- **Target Platform**: Windows (60 FPS minimum)

---

## Milestone Overview

| Milestone | Name | Dependencies | Status |
|-----------|------|--------------|--------|
| M1 | Foundation & Setup | None | 🟢 Complete |
| M2 | Core Movement System | M1 | 🟢 Complete |
| M3 | Unit System | M2 | 🟢 Complete |
| M4 | Combat System | M3 | 🟢 Complete |
| M5 | Resource & Economy | M4 | 🟢 Complete |
| M6 | UI & Minimap | M5 | 🟢 Complete |
| M7 | Save/Load & Integration Tests | M6 | 🟢 Complete |
| M8 | Enemy AI Commander | M7 | 🔴 Not Started |
| M9 | Fog of War & Recon | M8 | 🔴 Not Started |
| M10 | Map Files & Terrain | M7 | 🔴 Not Started |
| M11 | Audio | M7 | 🔴 Not Started |
| M12 | Art & Animation | M7 | 🔴 Not Started |
| M13 | Command Depth & Balance | M8 | 🔴 Not Started |
| M14 | Main Menu & Game Shell | M7 | 🔴 Not Started |

---

## Milestone M1: Foundation & Setup

### Goals
- [x] Set up premake5 build system with msbuild integration
- [x] Configure project structure (src/game, tests)
- [x] Integrate glm for math operations
- [x] Implement raygui for UI framework
- [x] Set up event dispatcher architecture
- [x] Create ECS-lite component-based architecture
- [x] Configure debug/release profiles with assertions

### Implementation Steps
1. **Premake5 Setup**
   - Generate `premake5.lua` with Windows targets
   - Configure `.prj` files for game and tests
   - Set up msbuild solution generation

2. **Project Structure**
   ```
   src/game/
   ├── main.cpp (entry point)
   ├── public/
   │   ├── Game.h
   │   ├── Unit.h
   │   ├── ResourceSystem.h
   │   └── Event.h
   └── private/
       ├── Game.cpp
       ├── Unit.cpp
       ├── ResourceSystem.cpp
       └── Event.cpp
   tests/
   ├── unit-tests/
   └── integration-tests/
   ```

3. **Architecture Components**
   - Create Event Dispatcher base class
   - Implement ECS-lite registry for component management
   - Set up basic Game manager with lifecycle hooks

### Blockings
- None identified

### Deliverables
- ✅ `premake5.lua` configuration file
- ✅ Project folder structure created
- ✅ Base architecture skeleton ready

---

## Milestone M2: Core Movement System

### Goals
- [x] Implement tile-based movement system (64x64 tiles)
- [x] Unit snapping to tile grid
- [x] Manual camera panning with WASD keys
- [x] Mouse selection and command inputs
- [x] Keyboard shortcut support
- [x] Basic input handling framework

### Implementation Steps
1. **Tile Map System**
   - Create TileMap class for 64x64 grid representation
   - Implement tile collision detection
   - Add terrain type definitions (grass, water, building)

2. **Unit Movement Component**
   - Extend Unit struct with movement component
   - Implement tile snapping logic
   - Add pathfinding on tile grid (A* or waypoint system)
   - Handle unit bounds and obstacles

3. **Camera System**
   - Implement manual panning camera
   - Bind WASD keys to camera movement
   - Add mouse drag for viewport control
   - Support scroll wheel zoom

4. **Input Handling**
   - Create InputManager class
   - Map mouse clicks to unit selection
   - Implement keyboard shortcuts registry
   - Handle command queueing system

### Blockings
- ⚠️ Need placeholder 32x32 pixel art sprites for units
- ⚠️ Tile map file format definition needed (binary/text?)

---

## Milestone M3: Unit System

### Goals
- [x] Implement Unit component with all attributes
- [x] Create unit type definitions (7 types)
- [x] Implement basic AI behaviors (pathfinding, targeting)
- [x] Formation tactics for unit groups
- [x] Unit state management (idle, moving, attacking)

### Implementation Steps
1. **Unit Component Design**
   - Define Unit struct with:
     ```cpp
     struct Unit {
         float health;
         int armorType;
         int attackPower;
         int attackRange;
         float cooldown;
         Vector2 position;  // snapped to 64x64 grid
         Vector2 velocity;
         bool isSelected;
         int teamID;
         UnitType type;
     };
     ```

2. **Unit Type Registry**
   - Create enum for 7 unit types:
     - Infantry
     - Anti-Armor Infantry
     - Engineer
     - IFV
     - Artillery
     - Light Tank
     - Heavy Tank
   - Define base stats for each type

3. **AI Component**
   - Implement simple pathfinding (tile grid A*)
   - Add targeting logic (nearest enemy, threat priority)
   - Create formation behavior (spacing, alignment)
   - State machine: Idle → Moving → Attacking

4. **Unit Factory**
   - Create spawner for unit instantiation
   - Handle resource cost validation
   - Manage unit lifecycle events

### Blockings
- None identified
- ⚠️ Unit balance numbers need reference from Command & Conquer / Company of Heroes

---

## Milestone M4: Combat System

### Goals
- [x] Implement damage calculation (attack type + armor type)
- [x] Create 2D hitbox system with raylib sprites
- [x] Add attack animations and cooldowns
- [x] Prevent infantry crushing by vehicles/tanks
- [x] Attack range checking
- [x] Damage feedback visualization

### Implementation Steps
1. **Damage System**
   - Define damage/armor types (inspired by C&C, CoH):
     ```cpp
     enum DamageType { KINETIC, EXPLOSIVE, ENERGY };
     enum ArmorType { STEEL, RUBBER, COMPOSITE };
     ```
   - Implement damage matrix lookup
   - Calculate effective damage with type multipliers

2. **Combat Resolution**
   - Create simplified 2D hitbox system
   - Implement attack range checking (circle/radius)
   - Add projectile vs instant-hit logic per unit type

3. **Animation System**
   - Create AttackState enum (Idle, WindUp, Hit, Recover)
   - Implement cooldown tracking per action
   - Add visual feedback (damage numbers, hit particles)

4. **Anti-Crush Mechanism**
   - Flag infantry units as "anti-vehicle" protected
   - Modify damage matrix to prevent vehicle→infantry crush damage
   - Alternative: add armor bonus for infantry against vehicles

### Blockings
- ⚠️ Need 32x32 placeholder sprites for each unit type (attack frames)
- ⚠️ Damage balance numbers need tuning after initial implementation

---

## Milestone M5: Resource & Economy

### Goals
- [x] Implement iron and oil resource system (no cap)
- [x] Create base building mechanics
- [x] Implement resource node gathering (Company of Heroes style)
- [x] Base income generation
- [x] Building placement on tile grid
- [x] Unit production queue

### Implementation Steps
1. **Resource Manager**
   - Create ResourceManager class
   - Track iron and oil counts
   - No maximum cap enforcement
   - Periodic refresh for node spawns

2. **Building System**
   - Define building types (Base, Resource Depot, Factory)
   - Implement placement validation (tile grid, no overlap)
   - Set up base income timer system
   - Create building state machine

3. **Resource Nodes**
   - Spawn resource nodes on map
   - Implement gathering logic (units → node → transfer)
   - Handle node depletion and respawn

4. **Production System**
   - Create unit production queue
   - Validate resource costs before spawning
   - Track production progress

### Blockings
- ⚠️ Need building placement sprites for 32x32 tiles
- ⚠️ Resource node icons needed

---

## Milestone M6: UI & Minimap

### Goals
- [x] Implement raygui-based UI framework
- [x] Create minimap (unit positions only, periodic refresh)
- [x] Display resource counters (iron/oil)
- [x] Unit selection highlighting
- [x] Keyboard shortcut overlays
- [x] Game menu system

### Implementation Steps
1. **UI Framework Setup**
   - Integrate raygui with raylib window
   - Create UI container management
   - Implement event-driven UI updates

2. **Minimap Implementation**
   - Render scaled-down tile map (periodic refresh)
   - Draw unit positions as small markers
   - Don't show terrain features (unit-only)
   - Implement viewport sync with main camera

3. **HUD Components**
   - Create resource display panel (top-left)
   - Add selection info panel (center-bottom)
   - Implement command/shortcut hints

4. **Menu System**
   - Pause menu
   - Game over screen
   - Settings panel

### Blockings
- None identified
- ⚠️ Need placeholder UI icons for resources and units

---

## Milestone M7: Save/Load & Integration Tests

### Goals
- [x] Implement save game functionality
- [x] Implement load game functionality
- [x] Create unit tests for all core components
- [x] Create integration tests for game loop
- [x] Performance profiling (60 FPS target validation)

### Implementation Steps
1. **Save/Load System**
   - Serialize game state to binary file
   - Include: units, buildings, resources, camera position
   - Implement deserialization with versioning
   - Add save slot management

2. **Unit Tests (Core Systems First)**
   - Unit collision tests
   - Pathfinding logic tests
   - Resource calculation tests
   - Damage matrix tests
   - Event dispatcher tests

3. **Integration Tests**
   - Game loop interactions
   - Multi-unit combat scenarios
   - Resource economy cycles
   - Save/load roundtrip tests

4. **Profiling & Optimization**
   - Add FPS counter with frame budgeting
   - Profile unit count performance (up to 200 units)
   - Optimize rendering batch operations

### Blockings
- ⚠️ Need binary serialization format decision
- ⚠️ Test framework setup time

---

## Milestone M8: Enemy AI Commander

Rationale: every system so far serves a skirmish against a scripted demo enemy. A real RTS needs an AI opponent that builds, expands, and attacks (Q52 already committed to Easy/Medium/Hard difficulties).

### Goals
- [ ] Implement AI build-order logic (base → harvesters → factory → army)
- [ ] Implement timed attack waves with rally + launch logic
- [ ] Add basic scouting (periodic scout unit toward player base)
- [ ] Implement Easy / Medium / Hard difficulty levels
- [ ] AI plays by the same economy rules as the player (no free resources on Medium)

### Implementation Steps
1. **AI Commander**
   - Create AICommander class owning a teamID, build-order queue, and wave timer
   - Reuse ProductionQueue + UnitFactory: AI spends from its own ResourceSystem
   - Place buildings via PlaceBuilding on its side of the map
2. **Waves & Scouting**
   - Form squads with IssueFormationMove to a rally tile, then order the attack
   - Send a cheap fast unit toward the player base on a timer; remember last-seen position
3. **Difficulties**
   - Easy: slow build order, small waves, no retreat, attacks nearest target only
   - Medium: full build order, mixed-composition waves, re-targets via AcquireTarget
   - Hard: faster timers, counter-composition picks, retreats losing fights
4. **Tests**
   - Headless skirmish: AI vs AI for N frames, assert both economies grow and a wave launches
   - Difficulty unit tests: wave size/timers scale monotonically Easy → Hard

### Blockings
- ⚠️ Difficulty tuning needs playtesting after initial implementation

---

## Milestone M9: Fog of War & Recon

Rationale: sight ranges exist (M3) but all units see everything — no scouting value, no ambushes, contrary to the Dune 2000 reference (Q5). Fog makes recon units and map control meaningful.

### Goals
- [ ] Implement per-team visibility grid over the TileMap
- [ ] Distinguish unexplored (black) vs explored-but-unseen (dimmed, frozen snapshot)
- [ ] Hide enemy units/buildings outside vision in world view and minimap
- [ ] Gate targeting: units cannot acquire targets they cannot see
- [ ] Give scout-role units/API a sight advantage (fast units, cheap cost)

### Implementation Steps
1. **Visibility System**
   - Create FogOfWar class: per-team explored + visible bitsets, recompute from unit sightRange each tick (stagger teams across frames if slow)
   - Render shroud overlay in world view; dim explored tiles, black out unexplored
2. **Integration**
   - Minimap shows only visible enemies; viewport box unchanged
   - AcquireTarget skips unseen enemies; UpdateUnit cannot chase unseen targets
   - Save/load includes explored sets (version bump to v2)
3. **Tests**
   - Visibility unit tests: unit reveals radius, obstacle behavior per design answer, explored persists after leaving
   - Integration: blind AI fails to engage a hidden force, scout reveals it

### Blockings
- ⚠️ Open design questions (re-shroud? vision blockers? blind artillery?) — see Q_AND_A 75–77

---

## Milestone M10: Map Files & Terrain

Rationale: maps are hardcoded in main.cpp, yet Q46/Q54/Q73 already decided text-format map files with a metadata header loaded from `data/` at runtime. Terrain movement costs were also approved (Q47/Q55, uniform start).

### Goals
- [ ] Define text `.map` format with header (version, dimensions, author notes)
- [ ] Implement map loader from `data/` (terrain legend, water/building tiles, node + spawn markers)
- [ ] Support terrain movement costs (uniform at first, per-type multipliers ready)
- [ ] Ship 2+ hand-made maps using choke points and separated starting bases
- [ ] Demo loads a map file instead of hardcoded tiles

### Implementation Steps
1. **Format + Loader**
   - Create MapFile parser: 5-line header (comment, name, author, width, height), then ASCII grid rows; grid/header mismatch rejected
   - Legend (per Q79): `.` grass, `~` water, `T` forest, `^` rock, `I`/`O` iron/oil nodes, `1`/`2` player/AI spawns (grass only), `B` pre-placed building
2. **Costs & Content**
   - Add per-terrain cost multiplier to Pathfinder (uniform 1.0 initially)
   - Author 2 maps with choke points per RTS level-design practice
     (per Q79: Crossroads + Twin Basins, 24x18, mirror-symmetric, spawns on grass)
3. **Tests**
   - Loader unit tests: header parse, legend errors rejected, dimension mismatch rejected
   - Roundtrip: save-game on loaded map reloads identically

### Blockings
- ⚠️ Tile legend characters need sign-off — see Q_AND_A 78

---

## Milestone M11: Audio

Rationale: Q22 pointed at rfxgen for SFX and it is already in `libs_deps.json`, but nothing plays a sound. Combat/economy feedback is visual-only.

### Goals
- [ ] Integrate rfxgen-generated SFX (select, order confirm, attack, explosion, building placed, node depleted, victory/defeat)
- [ ] Add background music loop with volume control
- [ ] Add volume/mute settings to the pause menu (persist in save file or settings file)
- [ ] Audio must not block the main thread on load/play

### Implementation Steps
1. **SFX Pipeline**
   - Generate `.wav` set with rfxgen, load via raylib audio at startup
   - Hook UnitSpawned/UnitDestroyed/ResourceChanged events to sounds
2. **Music + Settings**
   - Stream a looped track; MenuSettings gains master/music/sfx volumes + mute
3. **Tests**
   - Headless-safe: audio init guarded so test binary runs without a device
   - Settings roundtrip through save/load

### Blockings
- ⚠️ Music source undecided (generated loop vs composed track) — see Q_AND_A 79

---

## Milestone M12: Art & Animation

Rationale: every blocking table since M2 says "colored rectangles". Mechanics are done; the game still looks like a prototype.

### Goals
- [ ] Create 32x32 spritesheets for all 7 unit types (idle + attack frames minimum)
- [ ] Create building sprites (Base/Depot/Factory), node icons (iron/oil), UI icons
- [ ] Implement sprite animation (attack windup ↔ Hit timing from M4 phases)
- [ ] Add muzzle-flash / explosion particles on ResolveAttack hits
- [ ] Keep rectangle fallback behind a flag for headless tests

### Implementation Steps
1. **Pipeline**
   - Decide spritesheet layout + authoring tool, load via raylib textures
   - Unit renderer maps UnitType + AttackPhase → frame
2. **Effects**
   - Particle burst on damage (count scales with effective damage), flash on node deplete
3. **Tests**
   - Animation unit tests: phase→frame mapping, fallback flag honored
   - Perf re-run: textures must not regress the 200-unit budget

### Blockings
- ⚠️ Art ownership undecided (hand-made vs generated vs commissioned) — see Q_AND_A 80

---

## Milestone M13: Command Depth & Balance

Rationale: the order set is move-only; genre basics like attack-move, stances, and repair are missing, and stat/cost numbers are still M3 placeholders. This milestone turns the sandbox into a playable game.

### Goals
- [ ] Implement attack-move (engage on contact, resume path after)
- [ ] Implement unit stances (hold position / guard / patrol)
- [ ] Implement Engineer repair (heal buildings + mechanical units for resources)
- [ ] Add interactive production UI (build menu bound to Factory, rally-point placement)
- [ ] Add minimap click-to-move camera + enforce camera zoom limits (Q53)
- [ ] Multi-slot save UI (quicksave exists; add named slots)
- [ ] Full balance pass over costs, build times, damage matrix, and AI difficulties

### Implementation Steps
1. **Orders**
   - Extend Unit state driver with attack-move path + stance field; patrol as looping waypoint pair
   - Repair as a channeled action consuming iron over time
2. **UI**
   - Factory build panel (raygui): queue display, cancel, rally set by click
   - Save/load menu with 3+ named slots over the v1/v2 format
3. **Balance**
   - Spreadsheet all unit/building numbers; tune so every unit class has a role and META shifts with tech (per research guidance)
   - AI-vs-AI soak tests assert no stalemates and sub-60s average skirmish end

### Blockings
- ⚠️ Balance targets need sign-off (mirror factions vs asymmetric later?) — see Q_AND_A 81–83

---

## Milestone M14: Main Menu & Game Shell

Rationale: there is no main menu — the game boots straight into the demo skirmish. M6 built pause, game-over/victory, and settings overlays, but no title screen, skirmish setup, or shell around the match. This milestone adds the boot-to-menu flow and wires it to the other milestones' outputs (map list from M10, difficulty from M8, volumes from M11, save slots from M13).

### Goals
- [ ] Boot to a title screen (not directly into battle); Start Skirmish / Load Game / Settings / Quit
- [ ] Skirmish setup screen: map select (lists `data/*.map`), difficulty select (Easy/Medium/Hard), player color/faction display
- [ ] Settings screen: camera speed, minimap toggle, volumes + mute (shared with pause menu)
- [ ] Load-game entry listing available save slots; Quit exits cleanly
- [ ] Menu navigation fully testable headless (state machine without raygui calls)

### Implementation Steps
1. **Menu State**
   - Extend menu flow with `MenuState::MainMenu` + setup/settings sub-states; game world only builds after Start confirms
   - main.cpp boot flow: menu first, world construction on Start, teardown back to menu on Quit-to-menu
2. **Setup Screen**
   - Enumerate maps from `data/`; show author/dimensions from the M10 header
   - Difficulty picker stores the choice for the M8 AICommander; gray out options whose milestones are missing
3. **Settings + Slots**
   - Unify pause-menu and main-menu settings on one MenuSettings struct, persisted to disk
   - Load screen reuses the M13 slot UI; empty slots shown disabled
4. **Tests**
   - Menu-flow unit tests: boot state, setup validation (no map = Start disabled), settings roundtrip
   - Integration: headless Start with map + difficulty builds a simulating world

### Blockings
- ⚠️ Skirmish option scope (map + difficulty only, or also starting resources/team count?) — see Q_AND_A 85–86

---

## Summary of Known Blockings

| Blocking | Required For | Resolution Needed |
|----------|---------------|-------------------|
| 32x32 placeholder sprites | M12 | Decide art ownership (Q_AND_A 80), then produce unit/building/node sheets |
| Unit balance numbers | M13 | Spreadsheet pass + AI soak tests (Q_AND_A 81–83) |
| Tile map format | M10 | Legend sign-off (Q_AND_A 78), then loader |
| UI icons | M12 | Bundle with art milestone |
| Music source | M11 | Generated loop vs composed track (Q_AND_A 79) |
| Fog rules | M9 | Re-shroud, vision blockers, blind fire (Q_AND_A 75–77) |

---

## Next Steps

1. **Immediate**: Answer the M8–M14 open questions in `plans/Q_AND_A.md` (74–86)
2. **Short-term**: Begin M8 (Enemy AI Commander) — first milestone needing an opponent
3. **Content**: M10 (maps) and M12 (art) are parallelizable once format/pipeline questions are settled
