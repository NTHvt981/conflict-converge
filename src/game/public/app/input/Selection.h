#pragma once

#include <utility>

#include "raylib.h"

#include "app/ui/GameCamera.h"
#include "core/Registry.h"
#include "core/Team.h"

enum class BuildingType;
enum class UnitType;

// Hit test is the unit's tile box at its snapped position.

Entity PickUnitAt(Registry &registry, Vector2 worldPos);
void SelectOnly(Registry &registry, Entity entity);
void DeselectAll(Registry &registry);

// First selected unit, or kInvalidEntity.
Entity SelectedUnit(Registry &registry);
int SelectedUnitCount(Registry &registry);

Rectangle NormalizeRect(Vector2 a, Vector2 b);

Rectangle DraggedWorldBox(const GameCamera &camera, Vector2 screenStart, Vector2 screenEnd);
// No normalization: a line has direction, unlike a box.
std::pair<Vector2, Vector2> DraggedWorldLine(const GameCamera &camera, Vector2 screenStart,
											 Vector2 screenEnd);

// Every unit whose body center is inside worldBox; add=false replaces, true
// extends. Returns the picked count.
int SelectInRect(Registry &registry, Rectangle worldBox, bool add);

// Control groups: bit N of Unit::controlGroups. Assign replaces membership,
// AddTo stamps the bit, Recall selects exactly the bit's members.
int AssignControlGroup(Registry &registry, int groupBit);
int AddToControlGroup(Registry &registry, int groupBit);
int RecallControlGroup(Registry &registry, int groupBit);

int SelectAllOfType(Registry &registry, UnitType type, TeamId teamID, bool add);
int SelectAllOfTypeInRect(Registry &registry, Rectangle worldViewport, UnitType type,
						  TeamId teamID, bool add);
int SelectAllBuildings(Registry &registry, BuildingType type, TeamId teamID);
int SelectAllProductionBuildings(Registry &registry, TeamId teamID);
