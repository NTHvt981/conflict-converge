#pragma once

#include <utility>

#include "raylib.h"

#include "app/ui/GameCamera.h"
#include "core/Registry.h"

enum class BuildingType;
enum class UnitType;

// Mouse selection (left-click pick) over the unit registry. Hit test is the
// unit's 64x64 tile box at its snapped position.

Entity PickUnitAt(Registry &registry, Vector2 worldPos);
void SelectOnly(Registry &registry, Entity entity);
void DeselectAll(Registry &registry);

// First selected unit, or kInvalidEntity (right-click orders target this).
Entity SelectedUnit(Registry &registry);
int SelectedUnitCount(Registry &registry);

Rectangle NormalizeRect(Vector2 a, Vector2 b);

Rectangle DraggedWorldBox(const GameCamera &camera, Vector2 screenStart, Vector2 screenEnd);
// No normalization: a line has direction, unlike a box.
std::pair<Vector2, Vector2> DraggedWorldLine(const GameCamera &camera, Vector2 screenStart,
                                             Vector2 screenEnd);

// Box-select every unit whose body center falls inside worldBox; add=false
// replaces, true extends. Returns how many units the box picked.
int SelectInRect(Registry &registry, Rectangle worldBox, bool add);

// QoL control groups (bit N of Unit::controlGroups = member of group N).
// Assign replaces membership; AddTo stamps the bit without clearing existing
// members; Recall selects exactly the bit's members.
int AssignControlGroup(Registry &registry, int groupBit);
int AddToControlGroup(Registry &registry, int groupBit);
int RecallControlGroup(Registry &registry, int groupBit);

int SelectAllOfType(Registry &registry, UnitType type, int teamID, bool add);
int SelectAllOfTypeInRect(Registry &registry, Rectangle worldViewport, UnitType type,
                          int teamID, bool add);
int SelectAllBuildings(Registry &registry, BuildingType type, int teamID);
