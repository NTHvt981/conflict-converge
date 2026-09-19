#pragma once

#include <utility>

#include "raylib.h"

#include "GameCamera.h"
#include "Registry.h"

enum class BuildingType;
enum class UnitType;

// Mouse selection (left-click pick) over the unit registry. Hit test is the
// unit's 64x64 tile box at its snapped position.

Entity PickUnitAt(Registry &registry, Vector2 worldPos);
void SelectOnly(Registry &registry, Entity entity);
void DeselectAll(Registry &registry);

// First selected unit, or kInvalidEntity (right-click orders target this).
Entity SelectedUnit(Registry &registry);
// How many units are selected (portrait panel shows a count for N > 1).
int SelectedUnitCount(Registry &registry);

// Normalize a drag box (either corner may lead) to positive width/height.
Rectangle NormalizeRect(Vector2 a, Vector2 b);

// Screen-space drag (start/end in screen pixels) into a normalized
// world-space rectangle via the camera. Shared seam for box-select, line
// formation, and area commands.
Rectangle DraggedWorldBox(const GameCamera &camera, Vector2 screenStart, Vector2 screenEnd);
// Screen-space drag endpoints into world space (no normalization — a line
// has direction, unlike a box). For line-draw formation placement.
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

// QoL idle selection: every same-team unit with nothing to do.
// workersOnly=true restricts to Engineers; false selects the idle army.
int SelectIdle(Registry &registry, int teamID, bool workersOnly);
// Non-mutating count for HUD buttons.
int CountIdle(const Registry &registry, int teamID, bool workersOnly);

// QoL select-all-of-type: every same-team unit of `type`; add=false replaces.
int SelectAllOfType(Registry &registry, UnitType type, int teamID, bool add);
// QoL double-click: same filter, restricted to the world-space viewport rect.
int SelectAllOfTypeInRect(Registry &registry, Rectangle worldViewport, UnitType type,
                          int teamID, bool add);
// QoL select-all-buildings: every same-team building of `type`.
int SelectAllBuildings(Registry &registry, BuildingType type, int teamID);
