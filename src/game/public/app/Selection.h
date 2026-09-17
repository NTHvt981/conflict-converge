#pragma once

#include <utility> // std::pair for DraggedWorldLine

#include "raylib.h" // Vector2, Rectangle

#include "GameCamera.h" // ScreenToWorld for drag conversion
#include "Registry.h" // Entity, kInvalidEntity

enum class BuildingType; // fwd-decl (Selection.cpp includes Building.h)
enum class UnitType; // fwd-decl (Selection.cpp includes Unit.h)

// M2 Goal 4: mouse selection (left-click pick) over the unit registry.
// Hit test is the unit's 64x64 tile box at its snapped position.

Entity PickUnitAt(Registry &registry, Vector2 worldPos);
void SelectOnly(Registry &registry, Entity entity);
void DeselectAll(Registry &registry);

// First selected unit, or kInvalidEntity (right-click orders target this).
Entity SelectedUnit(Registry &registry);

// Normalize a drag box (either corner may lead) to positive width/height.
Rectangle NormalizeRect(Vector2 a, Vector2 b);

// Converts a screen-space drag (start/end in screen pixels) into a
// normalized world-space rectangle, via the camera's current view.
// Shared seam for box-select, line formation, and area commands.
Rectangle DraggedWorldBox(const GameCamera &camera, Vector2 screenStart, Vector2 screenEnd);
// Screen-space drag endpoints into world space (no normalization — a line
// has direction, unlike a box). For line-draw formation placement.
std::pair<Vector2, Vector2> DraggedWorldLine(const GameCamera &camera, Vector2 screenStart,
                                             Vector2 screenEnd);

// Box-select every unit whose body center falls inside worldBox. With
// add=false the box replaces the selection, otherwise it extends it.
// Returns how many units the box picked.
int SelectInRect(Registry &registry, Rectangle worldBox, bool add);

// QoL control groups (bit N of Unit::controlGroups = member of group N).
// Assign replaces group bit's membership with the current selection;
// AddTo stamps the bit on selected units without clearing existing members;
// Recall selects exactly the bit's members (replacing selection).
// Returns how many units were stamped (assign/add) or selected (recall).
int AssignControlGroup(Registry &registry, int groupBit);
int AddToControlGroup(Registry &registry, int groupBit);
int RecallControlGroup(Registry &registry, int groupBit);

// QoL idle selection: selects every same-team unit with nothing to do
// (state Idle, no move/path/repair order). workersOnly=true restricts to
// Engineers (the sole gatherer type); false selects the idle army
// (everyone else). Channeling repair-Engineers also report state Idle,
// so hasRepairOrder is checked explicitly — busy repair crews are never
// flagged idle. Replaces the selection; returns how many were picked.
int SelectIdle(Registry &registry, int teamID, bool workersOnly);
// Non-mutating count for HUD buttons (live counts without side effects).
int CountIdle(const Registry &registry, int teamID, bool workersOnly);

// QoL select-all-of-type: every same-team unit of `type`. add=false
// replaces (like SelectInRect), true extends. Returns picked count.
int SelectAllOfType(Registry &registry, UnitType type, int teamID, bool add);
// QoL double-click: same team/type filter as SelectAllOfType, additionally
// restricted to the world-space viewport rect (body-center containment,
// exactly like SelectInRect, so both agree on "in this rect").
int SelectAllOfTypeInRect(Registry &registry, Rectangle worldViewport, UnitType type,
                          int teamID, bool add);
// QoL select-all-buildings: every same-team building of `type` (minimal
// building-selection groundwork for the production hotkey and future
// per-building UI). Replaces building selection; returns picked count.
int SelectAllBuildings(Registry &registry, BuildingType type, int teamID);
