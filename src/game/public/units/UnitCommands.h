#pragma once

#include <vector>

#include "Registry.h"
#include "Unit.h"

class TileMap;
class OccupancyGrid;

// Selection-batch orders shared by hotkeys and HUD ability buttons.
// Each command applies to every selected unit; map-dependent ones route
// through IssueOrEnqueue like the right-click path.
void SetSelectionStance(Registry &registry, Stance stance);
void HaltSelection(Registry &registry);
void ToggleSelectionAutoRetreat(Registry &registry);
int IssueSelectionAttackMove(Registry &registry, const TileMap &map, OccupancyGrid *occ,
                             Vector2 dest, bool queued);
int IssueSelectionPatrol(Registry &registry, const TileMap &map, OccupancyGrid *occ,
                         Vector2 dest, bool queued);
int IssueSelectionRepair(Registry &registry, const TileMap &map, OccupancyGrid *occ,
                         Entity target, bool queued);
int IssueSelectionHeal(Registry &registry, const TileMap &map, OccupancyGrid *occ, Entity target,
                       bool queued);
