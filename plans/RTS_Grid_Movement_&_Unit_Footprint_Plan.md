RTS Grid Movement & Unit Footprint Plan

0. Current Codebase Fit & Prerequisites (review notes)

This plan is scope, not a refinement: all 7 existing unit types (Infantry,
AntiArmorInfantry, Engineer, IFV, Artillery, LightTank, HeavyTank) are
currently hard 1x1, and the milestone spec (plans/MILESTONES.md) already
reports M1-M15 complete. What's already there vs. what this plan assumes:

- Buildings already have multi-tile footprints (Building.h: Footprint(),
  PlaceBuilding marks every footprint tile TerrainType::Building) but
  TileMap has no per-tile occupant reference back to the entity -- only a
  terrain enum. The "Tile -> Building ID" map in section 7 does not exist
  yet and is a real prerequisite for sections 16-17 (entrances/attack
  positions), not something later phases can assume is already there.
- Units have no occupancy grid today. Unit-vs-unit collision is
  SeparateUnits(), a continuous push-apart pass on 32px body overlap --
  not grid occupancy. This plan's occupancy[x][y] = entity model (section
  8) and SeparateUnits will fight each other unless Phase 4 states
  explicitly which one owns unit-vs-unit collision (recommendation: grid
  occupancy owns entering a new tile / CanEnter; SeparateUnits keeps
  smoothing overlap within a tile so units don't visually stack).
- Pathfinder is 4-directional only today, no diagonals, so corner-cutting
  (section 12) is a genuinely new problem being introduced, not an
  existing one being fixed. src/game/private/units/Unit.cpp already has a
  known perf issue (attack-move re-pathing every frame while chasing --
  see docs/NOTES.md finding #4) -- fix that before Phase 4, since a
  footprint-aware 8-dir search is strictly more expensive per query and
  will make an unfixed per-frame re-path worse, not better.
- Registry::Create() recycles freed entity IDs with no generation/version
  counter (docs/NOTES.md finding #2). occupancy[x][y] = entity (section 8)
  is exactly the kind of long-lived ID reference that turns that gap into
  a real bug: a destroyed unit's stale ID left in an occupancy cell for
  even one frame can alias whatever unit spawns next with that recycled
  ID, corrupting CanEnter map-wide. Add the generation counter before or
  alongside Phase 4, not after.
- No squad/sub-unit concept exists. "Infantry" is a single Unit entity
  with one collidable position today (see section 14 note below).

1. Goal

Build a Dune 2000-style RTS movement system based on a shared tile grid.

The system must support:

Infantry and small 1×1 units

Large vehicles occupying multiple tiles

Buildings occupying multiple tiles

Smooth movement between grid positions

A* pathfinding

Terrain movement costs

Unit collision and blocking

Group/formation movement

Building entrances and attack positions

Future support for irregular unit footprints

The core principle is:

The map uses one shared grid. Units and buildings can occupy different numbers of tiles. Logical movement uses the grid; visual movement uses continuous world coordinates.

2. Coordinate System

Use integer coordinates for the logical map:

(x, y)


Example:

(3, 2)


represents column 3, row 2.

The grid should not be tied to a specific unit size.

Map
 ├── Tile (0,0)
 ├── Tile (1,0)
 ├── Tile (2,0)
 └── ...

World Coordinates

Units should also have floating-point world coordinates for smooth rendering.

Example:

Tile:
(3, 2)

World:
(112.5, 80.3)


If a tile is 32 world units:

worldX = tileX * tileSize
worldY = tileY * tileSize


The visual unit interpolates between tile positions instead of jumping from tile to tile.

3. Tile Data

Each tile should contain enough information for movement and occupancy.

Example:

Tile {
    x
    y

    terrain
    movementCost
    walkable

    occupiedBy
    buildingId
}


Possible terrain types:

SAND
ROCK
SPICE
DIFFICULT
CLIFF
WALL


Example:

Tile(10, 7)

terrain       = SAND
movementCost  = 1.0
walkable      = true
occupiedBy    = null
buildingId    = null

4. Unit Representation

Do not assume every unit occupies exactly one tile.

A unit should have:

Unit {
    id

    anchorX
    anchorY

    worldX
    worldY

    footprintWidth
    footprintHeight

    footprintMask

    movementSpeed
    movementType
}


The anchor identifies the unit's logical position.

The footprint determines how many tiles the unit occupies.

5. Unit Footprints

Example unit sizes (Dune 2000 reference, for illustration only):

Infantry       1×1
Trooper        1×1
Trike          1×1
Light Tank     2×2
Heavy Tank     2×2
MCV            3×3
Large Unit     4×4

Note: this project's actual roster is Infantry, AntiArmorInfantry,
Engineer, IFV, Artillery, LightTank, HeavyTank -- no MCV. Real footprint
sizes per type (does LightTank/HeavyTank become 2×2, or stay 1×1 and only
future vehicle types go larger?) need an explicit decision before Phase 4;
this table doesn't map onto the shipped unit list as-is.


A 2×2 Heavy Tank:

XX
XX


If its anchor is:

(10, 8)


it occupies:

(10,8) (11,8)
(10,9) (11,9)


The four tiles all reference the same unit ID.

(10,8) → Tank #17
(11,8) → Tank #17
(10,9) → Tank #17
(11,9) → Tank #17


The tiles are not four separate units.

6. Footprint Masks

Initially, rectangular footprints are sufficient:

1×1:

X

2×2:

XX
XX

3×3:

XXX
XXX
XXX

4×4:

XXXX
XXXX
XXXX
XXXX


Design the system so it can eventually support irregular masks:

XX.
XXX
.XX


This provides flexibility for future unit types.

7. Buildings

Buildings use the same grid as units but generally have larger footprints.

Example:

Building {
    id

    originX
    originY

    width
    height

    footprintMask

    entrances
}


A 3×2 building:

BBB
BBB


occupies six tiles.

Every occupied tile points back to the building:

Tile → Building ID


Buildings should generally be treated as blocking for movement.

8. Unit Occupancy

The map should maintain an occupancy system.

Example:

occupancy[x][y] = entity


Possible values:

EMPTY
UNIT
BUILDING
OBSTACLE


For a 2×2 tank:

occupancy[10][8] = Tank17
occupancy[11][8] = Tank17
occupancy[10][9] = Tank17
occupancy[11][9] = Tank17


This makes collision checks straightforward.

Note: the trickiest part of this plan is left unspecified -- how occupancy
is represented while a multi-tile unit is mid-transition between two
anchor tiles. A 2×2 tank moving diagonally straddles up to 6 cells during
interpolation, not the clean 4 cells it has at rest. Locking every covered
cell at all times risks the unit blocking its own trailing cells or
double-booking with another unit entering cells it's vacating; locking
only the anchor cell defeats the purpose of CanEnter for the rest of the
footprint. Decide this explicitly before Phase 4 (e.g.: occupancy is
reserved at the *destination* anchor tile only, footprint fully checked
via CanEnter at path-step time, and the tiles a unit is leaving are freed
the instant it starts moving off them rather than when it fully arrives).

9. Checking Whether a Unit Can Enter a Tile

Never check only the anchor tile for large units.

Instead:

CanEnter(anchorX, anchorY, unit)


checks the entire footprint.

For a 2×2 unit:

anchor = (10,8)

check:
(10,8)
(11,8)
(10,9)
(11,9)


The move is valid only if all required tiles are available.

Conceptually:

CanEnter(x, y, unit):
    for every cell in unit.footprint:
        if cell is outside map:
            return false

        if cell is blocked:
            return false

        if cell is occupied:
            return false

    return true

10. Pathfinding

Use A* for movement.

A 1×1 unit can use:

CanEnter(x, y)


A large unit uses:

CanEnter(x, y, footprint)


The pathfinder therefore searches for positions where the entire unit footprint fits.

Example:

########
#......#
#..##..#
#..##..#
#......#
########


A 1×1 infantry unit may pass through narrow spaces.

A 2×2 tank may not.

This naturally produces different movement routes for different unit sizes.

11. Neighbor Movement

Support 8-directional movement:

NW   N   NE

 W   X    E

SW   S   SE


Cardinal movement cost:

1.0


Diagonal movement cost:

1.414

Note: once diagonals exist, the A* heuristic must switch from Manhattan
distance (fine for the current 4-dir-only pathfinder) to octile/Chebyshev
distance, or it overestimates and the search is no longer admissible.

The pathfinder should also account for terrain movement costs.

Example:

Sand      = 1.0
Rock      = 1.0
Difficult = 2.0


The exact values should be data-driven.

12. Prevent Diagonal Corner Cutting

A large or small unit should not be allowed to move diagonally through an impossible corner.

Example:

# .
. X


The unit should not be allowed to move diagonally from X into the open tile if the two cardinal directions are blocked.

For large units, the same principle applies to the entire footprint.

13. Smooth Movement

A* produces a sequence of logical grid positions:

(3,2)
(3,3)
(4,4)
(5,4)


The unit should visually move between those points.

Do not teleport:

(3,2) → (3,3)


Instead interpolate:

world position
     ↓
smooth movement
     ↓
next waypoint


The grid controls the path.

The world position controls the animation.

14. Infantry — Squad Visual (concrete implementation spec)

Infantry stays a 1×1 logical unit. This is a render-layer-only feature:
one Unit entity (one position/velocity/health/collision body, unchanged)
draws as a cluster of soldier sprites instead of one sprite. It has no
dependency on Phases 1-8 and can be implemented and shipped independently
of the rest of this plan.

Scope: UnitType::Infantry, UnitType::AntiArmorInfantry, UnitType::Engineer
(the "foot" types). UnitType::IFV, Artillery, LightTank, HeavyTank keep
drawing as a single sprite, unchanged.

Current call site (this is the only place that needs to change):

  src/game/private/core/Game.cpp:940-956, inside
  registry.Each<Unit>([&](Entity, Unit &unit) { ... }):

    art.DrawUnit(unit.type, unit.teamID, FrameForPhase(unit.phase), unit.position);

  Name the currently-unnamed Entity lambda parameter (e.g. `id`) -- it's
  needed as the deterministic jitter seed below.

Steps:

1. Add a new pure function (suggested home: src/game/private/app/Art.cpp
   + a declaration in Art.h, since it's rendering-only and needs no
   access to Registry/TileMap):

     // Returns the number of soldier sprites to draw (1-6, driven by
     // healthFraction) and fills `outOffsets` with that many Vector2
     // pixel offsets from the unit's tile-corner position. Deterministic
     // per-entity (same `id` always produces the same layout) so it
     // doesn't need to be stored anywhere or included in save data.
     int SquadSlots(UnitType type, Entity id, float healthFraction,
                    std::array<Vector2, 6> &outOffsets);

   - Return early with count = 1, outOffsets[0] = {0,0} for any type not
     in scope above (so call sites can unconditionally call this and fall
     back to today's single-sprite behavior for vehicles).
   - Base layout: a fixed 6-slot cluster (e.g. two rows of offsets in the
     +/-6px to +/-10px range from center) sized to stay inside the
     existing 32x32 sprite inset (see body Rectangle at Game.cpp:947) so
     nothing spills into a neighbor tile.
   - Jitter: hash `id` (its integer value) with a small deterministic
     hash (same technique as the dithered camo hashing used in the voxel
     generator work) to perturb each slot by a few pixels, so squads
     don't look identical. No RNG state, nothing to save.
   - Soldier count from healthFraction: 6 slots at >83%, 5 at >67%,
     4 at >50%, 3 at >33%, 2 at >17%, 1 otherwise. Reuse the existing
     UnitHealthFraction(unit) helper already called at Game.cpp:960 --
     do not add a second health-fraction calculation.

2. At the call site, replace the single DrawUnit call with:

     std::array<Vector2, 6> slots;
     const int count = SquadSlots(unit.type, id, UnitHealthFraction(unit), slots);
     for (int i = 0; i < count; ++i)
     {
         art.DrawUnit(unit.type, unit.teamID, FrameForPhase(unit.phase),
                      { unit.position.x + slots[i].x, unit.position.y + slots[i].y });
     }

   All soldiers in a squad share one UnitFrame (computed once, as today)
   -- no per-soldier animation state.

3. Everything else at that call site (selection rectangle, health bar,
   damage-flash overlay, fog-of-war visibility check at Game.cpp:942-946)
   keeps keying off `unit.position`/`unit.isSelected` exactly as today --
   unchanged, since those still represent the one logical unit.

Explicitly out of scope (do not implement as part of this section):
individual soldier hit points, individual soldier pathing/collision,
soldiers persisting independently across save/load. The single Unit
entity remains the only simulated/collidable/pathable/savable object.

Definition of done: extend tests/unit-tests/art_tests.cpp with cases for
SquadSlots -- count buckets at each health-fraction threshold, count == 1
for non-infantry types, deterministic output for a repeated `id`, offsets
within the 32x32 inset bounds. No changes expected to movement_tests.cpp,
combat_tests.cpp, save_tests.cpp, or registry_tests.cpp -- if implementing
this requires touching any of those, scope has crept beyond this section.

15. Formation Movement

When multiple units are selected, don't give every unit exactly the same destination.

Instead, generate destination slots:

I     I     T T
I     I     T T


A formation system should account for each unit's footprint.

A 1×1 infantry unit requires less space than a 2×2 tank.

Therefore:

formation spacing
        +
unit footprint
        ↓
final unit positions

16. Buildings and Entrances

Buildings should expose specific interaction/entrance tiles.

Example:

###
###
#E#


Where:

E = entrance/access tile


Infantry should path to the entrance rather than attempting to path through the building footprint.

This can support:

Entering buildings

Capturing buildings

Repairing buildings

Unloading units

Interacting with structures

17. Building Attack Positions

For attacking buildings, generate valid tiles around the building footprint.

Example:

  X X X
  X X X
  X B X
  X B X
  X X X


Units can be assigned different attack positions.

This prevents every unit from trying to reach one exact tile.

Example:

      I   I   I
        ↓ ↓ ↓

     +---------+
 I → | BUILDING | ← I
     +---------+

        ↑ ↑
        I I

18. Line of Sight

Building footprints can also participate in visibility and targeting.

Buildings can block line of sight:

Infantry ---- Building ---- Enemy


The building's occupied tiles can therefore be used for:

Line-of-sight checks

Projectile collision

Targeting

Fog of war

Visibility

19. Large Unit Rotation

Initially, keep footprints axis-aligned.

For example:

2×3 unit

XXX
XXX


The visual model can rotate independently.

Avoid implementing rotated collision footprints until the basic movement system works.

Later, the system can support:

0°   → 2×3
90°  → 3×2
180° → 2×3
270° → 3×2


This should be an extension rather than a requirement for the first implementation.

20. Recommended Architecture
                    MAP
                     │
                     ▼
                 TILE GRID
                     │
        ┌────────────┴────────────┐
        │                         │
        ▼                         ▼
     BUILDINGS                  UNITS
        │                         │
   footprint                 footprint
        │                         │
        └────────────┬────────────┘
                     ▼
                OCCUPANCY
                     │
                     ▼
                PATHFINDING
                     │
                     ▼
             MOVEMENT SYSTEM
                     │
                     ▼
            WORLD COORDINATES
                     │
                     ▼
                 RENDERING

21. Core Design Rules

The implementation should follow these rules:

One shared tile grid for the entire game.

Tile coordinates are integer values.

World coordinates are floating-point values.

Every unit has an anchor position.

Every unit has a footprint.

Infantry normally uses a 1×1 footprint.

Large vehicles can use 2×2, 3×3, etc.

Buildings use multi-tile footprints.

Occupancy references entity IDs, not individual tiles as independent entities.

A unit can move only if its entire footprint fits.

A* operates on the shared grid.

Terrain contributes movement cost.

Visual movement is smooth and independent of the logical grid.

Formation movement accounts for different unit sizes.

Buildings expose entrance and attack positions.

Footprints should eventually support arbitrary masks.

22. Suggested Implementation Order
Phase 1 — Grid

Implement:

Tile
Grid
(x, y)
terrain
walkable

Phase 2 — 1×1 Movement

Implement:

Infantry
anchor position
world position
8-direction movement

Phase 3 — A*

Implement:

A*
terrain costs
blocked tiles
diagonal movement
corner-cutting prevention

Prerequisite (before Phase 4) — Fix Two Existing Gaps

Land these first; Phase 4 is exactly where both turn from a known issue
into active corruption of the new occupancy logic (see section 0):

Attack-move re-pathing every frame while chasing (docs/NOTES.md finding #4)
Entity ID generation/version counter in Registry (docs/NOTES.md finding #2)

Phase 4 — Multi-Tile Units

Add:

footprintWidth
footprintHeight
occupancy
CanEnter()


Test:

1×1
2×2
3×3
4×4

Phase 5 — Buildings

Add:

building footprints
building occupancy
entrances
attack positions

Phase 6 — Smooth Movement

Add:

waypoint interpolation
movement speed
turning
stopping

Phase 7 — Formation

Add:

destination slots
unit spacing
large-unit spacing
squad movement

Phase 8 — Advanced Features

Later add:

irregular footprints
unit rotation
line of sight
fog of war
advanced collision avoidance
formation reorganization

23. Final Mental Model

Think of the system as three separate layers:

LOGICAL GRID
────────────
"What tiles does this entity occupy?"

        ↓

PATHFINDING
───────────
"Can its footprint move there?"

        ↓

WORLD MOVEMENT
──────────────
"How does it smoothly get there?"


For example, a Heavy Tank might be:

Logical:

anchor = (10,8)
footprint = 2×2

XX
XX


Path:

(10,8)
(11,8)
(12,9)
(13,9)


Visual:

smoothly moves through those
waypoints in world space


This gives the game a single consistent grid while allowing infantry, tanks, MCVs, buildings, and future large units to have completely different physical sizes.

24. Implementation Handoff — File Map, Build, and Repo Conventions

This section exists so an implementing agent doesn't have to rediscover
the codebase from scratch. Read docs/NOTES.md first (known findings this
plan interacts with, esp. #2 and #4, called out in section 0).

File map (existing code each section/phase touches):

  Section 3 / Phase 1 (tile data, terrain)
    src/game/public/world/TileMap.h, private/world/TileMap.cpp
    Test: tests/unit-tests/tilemap_tests.cpp

  Section 4 / Phase 2 (unit position, movement)
    src/game/public/units/Unit.h, private/units/Unit.cpp
    Test: tests/unit-tests/movement_tests.cpp, unit_snap_tests.cpp

  Section 10-12 / Phase 3 (A*, diagonals, corner-cutting)
    src/game/public/world/Pathfinder.h, private/world/Pathfinder.cpp
    Test: tests/unit-tests/pathfind_tests.cpp

  Prerequisite fixes (before Phase 4)
    Attack-move re-path: src/game/private/units/Unit.cpp:378-404
      (see the detour branch calling IssuePathOrder every frame, ~line 392)
    Entity generation counter: src/game/private/core/Registry.cpp
    Test: extend registry_tests.cpp for the generation counter;
      orders_tests.cpp or a new attack_move perf assertion for the re-path fix

  Section 7-9 / Phase 4-5 (footprints, occupancy, buildings)
    src/game/public/economy/Building.h, private/economy/Building.cpp
    src/game/public/units/Unit.h (add footprint fields)
    src/game/public/world/TileMap.h (add occupant back-reference)
    Test: tests/unit-tests/building_tests.cpp; new footprint_tests.cpp
      for CanEnter()/occupancy, following the existing per-subsystem
      test-file convention (one *_tests.cpp per feature area)

  Section 13 / Phase 6 (smooth movement)
    src/game/private/units/Unit.cpp (UpdateUnitMovement, StepToward)
    Test: tests/unit-tests/movement_tests.cpp

  Section 14 (squad visual — see full spec above)
    src/game/private/core/Game.cpp:940-956 (call site)
    src/game/public/app/Art.h, private/app/Art.cpp (new SquadSlots)
    Test: tests/unit-tests/art_tests.cpp

  Section 15 / Phase 7 (formation)
    src/game/public/units/Formation.h, private/units/Formation.cpp
    Test: tests/unit-tests/formation_tests.cpp

  Section 16-18 (entrances, attack positions, line of sight)
    src/game/public/units/Combat.h, public/world/FogOfWar.h
    No dedicated test file yet; add one per the existing convention
      (e.g. entrances_tests.cpp) rather than folding into an unrelated file

  Section 22 save-data implications
    Any new persistent per-unit/per-tile state (footprint size if it's
    not derived from UnitType, occupancy if not derivable from unit
    positions on load) must go through src/game/private/app/SaveGame.cpp
    with the same InRange()/kMax* bounds-check discipline already used
    there -- see docs/NOTES.md finding #5 before adding any new tile
    coordinate field to the save format.

Build & test (Windows, Win32-only target — x64 fails):

  premake5 vs2022
  msbuild prj/ConflictConverge.sln /p:Configuration=Debug /p:Platform=Win32
  prj/bin/Debug/conflict-converge-test.exe

  IMPORTANT if invoking from a POSIX-style shell (Git Bash/MSYS): it
  rewrites leading-/ arguments like /p:Configuration=Release into garbage
  (observed turning /p:Configuration=Release into
  p:Configuration=Release and /m into a drive path M:/, which msbuild
  then rejects or silently no-ops). Run msbuild and cmake --build through
  a native shell (PowerShell or cmd.exe), not Git Bash, or the build will
  appear to succeed while doing nothing.

  Full first-time setup (protobuf/raylib/raygui/glm deps, if deps/ isn't
  already populated) is in README.md "Setup" and "Build".

Definition of done for the plan overall: conflict-converge-test.exe exits
0, including the new *_tests.cpp files added per phase above, and a
manual playtest (Start Skirmish) confirms mixed-footprint pathing (a
2x2+ unit routes around a gap a 1x1 unit can pass through) and squad
visuals render without changing selection/combat/save behavior.