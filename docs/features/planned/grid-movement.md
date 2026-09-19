# Grid Movement & Unit Footprints (planned)

Status: design doc, largely SUPERSEDED — most of it has since shipped
independently. Kept for the one open item and the prerequisite analysis.

The original proposal: a unified tile grid with multi-tile unit
footprints, an occupancy grid, footprint-aware 8-dir A* with
corner-cutting prevention, entrances/attack positions for buildings,
and smooth visual movement decoupled from the logical grid.

## Landed since

- 8-dir A* with octile heuristic and diagonal corner-cutting prevention
  (`Pathfinder.h`/`Pathfinder.cpp`; footprint-aware orders via
  `CanEnter`).
- Tile occupancy grid (`OccupancyGrid`, resynced on load).
- Building footprints, walkable entrances, and attack positions
  (`Building.h`: `Footprint`, `BuildingEntrances`,
  `BuildingAttackPositions`).
- 2x2 vehicle footprints via unit configs (`UnitConfig.h`).
- Recycled entity IDs carry generations (`Registry`), closing the
  stale-ID prerequisite.

## Open

- Arbitrary (non-rectangular) footprint masks.
