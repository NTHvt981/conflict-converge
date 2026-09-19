# Conflict Converge - Decision Log

Converted from the planning Q&A 2026-09-19: entries are kept verbatim as the decision record, with superseded answers annotated in place (Q48, Q84: protobuf era → cereal).

## Project Overview
- **Name**: Conflict Converge
- **Genre**: RTS (Real-Time Strategy)
- **Art Style**: 2D Pixel Art
- **Engine/Library**: raylib
- **Build System**: premake5 + msbuild (no CMake)

---

## Game Design & Mechanics Questions

### Core Gameplay
1. What is the maximum number of units that should be controllable simultaneously?
   Answer: 200
2. What unit types will be included in the initial release? (e.g., infantry, vehicles, tanks, support units)
   Answer: 
   - infantry
   - anti armor infantry
   - engineer
   - IFV (infantry fighting vehicle)
   - artilery
   - light tank
   - heavy tank
3. Will there be a turn-based or fully real-time combat system?
   Answer: fully real-time combat system
4. Are there specific RTS mechanics to implement? (e.g., base building, resource gathering, technology tree)
   Answer: implement base building and resource gathering
5. What is the intended scope of the map(s)? Any tile-based or free movement preference?
   Answer: tile-base movement, use Dune 2000 for reference

### Unit System
6. What attributes should units have beyond position, velocity, teamID, and isSelected? (health, armor, attack power, range, cooldowns, etc.)
   Answer: health, armor, attack power, range, cooldowns
7. What AI behaviors are needed for enemy units? (simple pathfinding, formation tactics, targeting logic)
   Answer: simple pathfinding, formation tactics, targeting logic
8. Should units have different movement types? (ground only, air support, ranged vs melee)
   Answer: only use ground

### Combat System
9. How should damage calculation work? (flat damage, percentage, armor penetration)
   Answer: use flat damage combine with attacker attack type and target armor type to calculate
10. Is there a specific combat resolution style? (raylib 2D sprites, simplified hitboxes)
    Answer: use raylib 2D sprites, simplified hitboxes
11. Should units have attack animations and cooldowns?
    Answer: yes

### Resources & Economy
12. What resources are needed for gameplay? (gold, wood, metal, energy, etc.)
    Answer: iron and oil
13. How should resource generation be balanced? (base income, resource nodes, upgrades)
    Answer: use Company of Heroes resource generation system as reference
14. What is the maximum resource cap per game session?
    Answer: No cap

### Camera & Controls
15. Should camera follow selected units or have manual panning?
    Answer: Use manual panning
16. Are there specific control schemes? (WASD movement, mouse clicks for selection/commands, keyboard shortcuts)
    Answer: Implement all 3: WASD movement, mouse clicks for selection/commands, keyboard shortcuts
17. Should there be a minimap or tactical overview mode?
    Answer: yes, there should be minimap

---

## Technical Implementation Questions

### Architecture
18. What is the preferred class hierarchy for Game components? (Game -> GameState -> UnitManager -> Unit)
    Answer: Use class hierarchy suitable for small scale game
19. Should the game use an event system for actions like unit attacks, deaths, resource changes?
    Answer: yes
20. Is there a preference for component-based architecture or traditional OOP design?
    Answer: prefer component-based architecture

### Dependencies
21. What external libraries should be included via premake (e.g., imgui for UI, glm for math)?
    Answer: Use glm for math, for UI, check https://github.com/raysan5/raygui
22. Are there specific raylib features to leverage? (particle systems, shaders, audio)
    Answer: For audio sfx, check https://github.com/raysan5/rfxgen

### Testing Strategy
23. Which components require unit tests? (unit collision, pathfinding logic, resource calculations)
    Answer: All components if possible
24. Should integration tests be created for game loop interactions?
    Answer: Yes, put integration tests in tests/integration-tests/

### Build Configuration
25. What are the target platforms? (Windows only, or cross-platform considerations)
    Answer: Windows only
26. Are there specific premake options to configure? (debug/release profiles, platform flags)
    Answer: debug/release profiles

---

## Art & Asset Requirements

27. Do you have existing pixel art assets, or should placeholder graphics be used during development?
   Answer: Use placeholder graphics for now
28. What is the expected tile/sprite size for 2D assets? (e.g., 32x32, 64x64)
    Answer: Use 32x32 for now
29. Are there specific color palettes or art style guidelines to follow?
   Answer: use art style Pixel art

---

## Milestone & Timeline Questions

30. What is the target release date or deadline?
    Answer: Currently no deadline
31. Should development be divided into milestones by feature (e.g., M1: Core Movement, M2: Combat, M3: UI)?
   Answer: Yes
32. Are there any mandatory features that must exist before a playable demo can be released?
    Answer: Core Movement, Combat and UI

---

## Additional Considerations

33. Should the game include save/load functionality?
    Answer: Yes
34. Will there be multiplayer or single-player only initially?
    Answer: only single-player
35. Are there specific performance targets? (FPS minimum, unit count limits)
   Answer: 60 FPS minimum
36. What is the expected content scope for initial release? (number of unit types, map count)
    Answer: Use small scope for now

---

## Additional Questions for Clarity

### Architecture & Class Hierarchy
37. For "class hierarchy suitable for small scale game" - should this mean:
    - A simplified version of Question 18's answer (lightweight, minimal nesting)?
    - Or a specific pattern like ECS-lite or hybrid OOP?
    - Should we keep the traditional structure but simplify each layer?
   Answer: use ECS-lite

38. For component-based architecture preference - what components are essential?
    - Unit (stats, AI behavior)
    - Resource system
    - Building/base construction
    - Camera/viewport
    - Input handling
    - Event dispatcher
    - UI manager
   Answer: Unit, Resource system

### Design & Systems
39. For "tile-based movement" (Dune 2000 reference):
    - What tile size? (e.g., 64x64 or 128x128)
    - Should units snap to tiles or move freely within tile grid?
   Answer: use 64x64 tile size and units should snap to tiles

40. For "use Company of Heroes resource generation":
    - Do you want the supply-based economy (supply limits vs resource points)?
    - Or just traditional iron/oil gathering without supply constraints?
    Answer: traditional iron/oil gathering without supply constraints

41. For damage calculation with attack types and armor types:
    - What are the preferred damage/armor types? (e.g., kinetic, explosive, energy vs steel, rubber, composite)
    - Any reference game for balance numbers?
   Answer: Command&Conquer, Company of heroes
   Note: Don't implement feature where infantry unit get crushed by vehicle/tank

42. For "minimap":
    - Should it show terrain features or just unit positions?
    - Real-time updates or periodic refresh?
    Answer: just unit positions, use periodic refresh

43. For "keyboard shortcuts" + "WASD movement" combination:
    - Should WASD control camera panning or unit movement?
    - What specific keyboard actions should exist beyond shortcuts?
    Answer: use WASD control for camera panning

### Testing & Build
44. For "debug/release profiles" in premake:
    - Any specific compiler flags needed? (e.g., assertions on, debug symbols)
    - Should we include performance profiling tools?
   Answer: Use assertions for Debug, debug symbols for all profiles

45. For "All components if possible" in testing:
    - What priority level for test coverage? (core systems first, UI second?)
    Answer: core systems first, UI second

---

## Additional Questions Needed

### Tile Map System
46. For the tile-based movement system, what file format do you prefer for storing the initial map layout?
    - Binary format (.bin) - faster load times, requires custom parser
    - Text format (.map/.txt) - human-readable, easier to edit manually
    - JSON format (.json) - structured and readable but adds dependency
   Answer: Text format (.map/.txt)

47. Should the tile map include terrain types (grass, water, forest, mountain) with different movement costs?
   Answer: Yes

### Save/Load System
48. For the save/load functionality, what binary serialization format do you prefer?
    - Custom binary (packed structs) - fastest but no human readability
    - Flatbuffer - Google's protocol buffer system
    - Protocol Buffers (protobuf) - well-established with schema definition
   Answer: Protocol Buffers (protobuf) — later superseded: migrated protobuf → header-only cereal 2026-09-19 (`CCB2`, v2, wire structs in `src/game/private/app/SaveWire.h`; see `docs/features/serialization.md`).

### Unit Balance Numbers
49. For Command & Conquer / Company of Heroes inspired balance numbers, should we:
    - Use approximate values from reference games as starting point
    - Create a simpler damage matrix for initial implementation
    - Start with very conservative numbers and tune up gradually
    Answer: Create a simpler damage matrix for initial implementation

### Placeholder Graphics Approach
50. For 32x32 placeholder graphics during development, what approach do you prefer?
    - Colored rectangles (fastest, simplest)
    - Simple generated pixel art patterns (medium complexity)
    - Use raylib built-in shapes with gradients (raylib-specific)
    Answer: Use Colored rectangles

### Unit Production Timing
51. For unit production in buildings:
    - Should units appear instantly when queued (no build time)?
    - Or should they have a production time before spawning?
    Answer: they should have a production time before spawning

### Enemy AI Difficulty Levels
52. Should the enemy AI have selectable difficulty levels (Easy, Medium, Hard)?
    - Easy: basic pathfinding, slow reaction
    - Medium: tactical targeting, formation tactics
    - Hard: aggressive behavior, counter-attacks
    Answer: Yes

### Camera Viewport Limits
53. For the manual camera panning system:
    - Should there be zoom limits (min/max viewport size)?
    - Or allow unlimited zoom with raylib?
    Answer: Should have zoom limits

### Tile Map Content Management
54. Should the tile map file include a header section for metadata (version, dimensions, author notes)?
    Answer: Yes

55. For terrain types with movement costs, should we define a default cost system or start with uniform costs?
    Answer: start with uniform costs

### Event System Design
56. For the event dispatcher mentioned in Q19 - should events be single-threaded (main thread only) or support multi-threading?
    Answer: Use single-threaded for now

57. What event categories are essential for the MVP? (unit events, resource events, game state events, UI events)
   Answer: All four — unit (spawn/destroy/damage), resource (changed/depleted), game-state (match start/pause/game-over/victory), and UI (menu actions, production ordered). All ride the existing single-threaded dispatcher as new EventType entries; M14's menu needs game-state + UI events.

### Data Structures
58. For the ECS-lite architecture - should entities be stored in a flat array with component bitsets or use separate arrays per component type?
    Answer: Use flat array with component bitsets

59. Should we use std::vector for dynamic collections or prefer custom arena allocators for performance-critical sections?
   Answer: use std::vector for dynamic collections

### Input System
60. For input handling - should we use raylib's built-in Input library with event polling or implement a state-based input manager?
    Answer: Use raylib's built-in Input library with event polling

61. Should keyboard shortcuts be bound to global actions (e.g., ESC = pause) or only within specific game contexts?
   Answer: keyboard shortcuts should be bound to global actions

---

## Build System & Dependencies

### Dependency Management
62. For dependencies in deps/ - should we use git submodules or standalone git clones?
    Answer: Use standalone git clones (to be more specific: put git config info in libs_deps.json, then run bootstrap.py to do the git cloning)

63. Should bootstrap.py be run automatically on first build or manually before running premake?
   Answer: bootstrap.py should be run manually

64. Should the premake configuration include steps to verify dependency integrity (e.g., checking for expected files)?
   Answer: yes

### Build Output Organization
65. Should the generated .vcxproj files place binaries in a single output folder or maintain separate folders per project?
    Answer: maintain separate folders per project

66. Should we include a post-build step to copy raylib.dll to the output directory (for DLL linking scenario)?
   Answer: yes

### Test Configuration
67. Should test executables be placed in a separate bin/ folder or alongside game binaries?
    Answer: put them in tests/bin/

68. Should premake generate separate .sln files for game and tests, or combine them into one solution?
    Answer: combine them into one solution

---

## Project Structure & File Organization

### Source Code Layout
69. How should source files be organized within src/? (e.g., src/core/, src/game/, src/components/)
    Answer: put them all in src/game/ for now

70. Should we create a public API header file that exposes only essential interfaces for testing?
    Answer: No

### Header File Organization
71. For component-based architecture - should each component have its own header or group related components together?
   Answer: Each component should have its own header

72. Should we use forward declarations extensively to reduce circular dependencies?
   Answer: Yes

### Resource Loading
73. Should tile maps and other resources be loaded from data/ folder at runtime or embedded in the binary?
    Answer: They should be loaded from data/ folder at runtime

---

## New Questions (M8–M14)

### Enemy AI (M8)
74. Should the AI play fair (same rules/fog as the player) or get handicaps? Research warns resource-cheating feels cheap — recommend fair rules + smarter build orders, with handicaps only on Easy (slower timers) if at all.
    Answer: Choose recommendation
75. What should trigger AI attack waves — fixed timers, army-size thresholds, or scouting intel (attack when it sees weakness)?
    Answer: only army-size thresholds should trigger AI attack waves for now

### Fog of War (M9)
76. When units leave an area, should it re-shroud to black or stay dimly visible with a frozen snapshot (StarCraft-style)? Recommend frozen snapshot — cheaper and standard.
    Answer: Choose recommendation
77. Should terrain block vision (forests/hills as blockers), or is sight pure radius? Pure radius is far simpler; blockers need a LOS pass.
    Answer: Use sight pure radius
78. Can artillery blind-fire into shrouded tiles? If yes, at what accuracy penalty?
    Answer: artillery should be able to blind-fire into shrouded tiles with no penalty

### Maps (M10)
79. For the `.map` text format: what legend characters for terrain/markers, and which 2 maps should ship first (size, choke points, starting positions)?
    Answer: Legend — `.` grass, `~` water (impassable), `T` forest (passable, cost 1.0 for now), `^` rock/mountain (impassable wall tile), `I`/`O` iron/oil node, `1`/`2` player/AI base spawn (must be grass), `B` pre-placed building tile (optional). Avoid `#` (looks like a comment) and lowercase (case bugs). Header — 5 lines: format comment, `name=`, `author=`, `width=`, `height=` (loader rejects grid/header mismatch). Ship 2 mirror-symmetric 24x18 maps: (1) Crossroads — player NW vs AI SE, vertical river with exactly 2 bridge crossings, home iron each + contested center oil; (2) Twin Basins — central lake, corner bases, one land-bridge route + one open route, teaches scouting and raiding. Spawns on grass, nodes ground-reachable, symmetric node distances.

### Audio (M11)
80. Music: generate a looped track (e.g., via tool) or commission/compose one? And what SFX list is mandatory for v1 beyond the baseline (select/order/attack/explosion/place/deplete/win/lose)?
    Answer: generate a looped track (e.g., via tool), just have baseline for now

### Art & Balance (M12–M13)
81. Art ownership: hand-made pixel art, AI-generated sprites, or commissioned? This decides the M12 pipeline and licensing.
    Answer: AI-generated pixel art
82. Balance target: mirror factions (same 7 units both sides, pure execution game) or asymmetric sides later? Recommend mirror for now — asymmetric doubles balance work.
    Answer: mirror for now
83. Should Engineer repair cost resources over time, and can Engineers capture enemy buildings (C&C-style)? Capture is a big mechanic — recommend repair-only for v1.
    Answer: repair only, repair should cost time but not resources

### Main Menu (M14)
85. Skirmish setup scope: map + difficulty only, or also starting resources, team count, and win condition (annihilation vs timed)? Recommend map + difficulty for v1 — the rest multiplies test surface.
    Answer: Recommend map + difficulty for v1
86. Settings persistence: separate settings file vs appended to the save format? Recommend a small standalone file so settings survive without a save.
    Answer: Use a small standalone file

### Plan Deviation Note
84. Q48 chose Protocol Buffers for serialization, but M7 shipped a custom versioned binary format (`CCSV` v1, now v2 with fog sets) for zero-dependency builds. Migrate to protobuf later, or ratify the custom format as final?
    Answer: Migrate to Protocol Buffers now — later superseded again: protobuf → header-only cereal 2026-09-19 (`CCB2`, v2; old `CCSV` and `CCPB` saves rejected, no back-compat shim).

---

## End of Q&A Document
