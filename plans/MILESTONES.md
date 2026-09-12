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
| M6 | UI & Minimap | M5 | 🔴 Not Started |
| M7 | Save/Load & Integration Tests | M6 | 🔴 Not Started |

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
- [ ] Implement raygui-based UI framework
- [x] Create minimap (unit positions only, periodic refresh)
- [ ] Display resource counters (iron/oil)
- [ ] Unit selection highlighting
- [ ] Keyboard shortcut overlays
- [ ] Game menu system

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
- [ ] Implement save game functionality
- [ ] Implement load game functionality
- [ ] Create unit tests for all core components
- [ ] Create integration tests for game loop
- [ ] Performance profiling (60 FPS target validation)

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

## Summary of Known Blockings

| Blocking | Required For | Resolution Needed |
|----------|---------------|-------------------|
| 32x32 placeholder sprites | M2, M4, M6 | Create simple pixel art placeholders or use colored rectangles |
| Unit balance numbers | M4 | Research C&C/CoH values, start with conservative estimates |
| Tile map format | M2 | Define binary vs text format preference |
| UI icons | M6 | Use raygui built-in shapes or create minimal icons |

---

## Next Steps

1. **Immediate**: Confirm blocking resolutions (sprite generation plan)
2. **Short-term**: Begin M2 implementation with placeholder graphics
3. **Testing**: Set up unit test framework during M2/M3 development
