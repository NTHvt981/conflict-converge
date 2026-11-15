#pragma once

#include <vector>

#include "core/Registry.h"
#include "units/Unit.h"

class TileMap;
class OccupancyGrid;

void SetSelectionStance(Registry &registry, Stance stance);
void HaltSelection(Registry &registry);
void ToggleSelectionAutoRetreat(Registry &registry);
void ToggleSelectionAutoRepair(Registry &registry);
int IssueSelectionAttackMove(Registry &registry, const TileMap &map, OccupancyGrid *occ,
							 Vector2 dest, bool queued);
int IssueSelectionPatrol(Registry &registry, const TileMap &map, OccupancyGrid *occ,
						 Vector2 dest, bool queued);
int IssueSelectionRepair(Registry &registry, const TileMap &map, OccupancyGrid *occ,
						 Entity target, bool queued);
int IssueSelectionHeal(Registry &registry, const TileMap &map, OccupancyGrid *occ, Entity target,
					   bool queued);
