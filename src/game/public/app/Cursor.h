#pragma once

#include "Registry.h"

#include "raylib.h"

struct Unit; // fwd-decl (Cursor.cpp includes Unit.h for team/type checks)

// Context-sensitive mouse cursors: what a right-click WOULD do if issued
// over worldPos right now. Pure prediction — calling this never changes
// orders, selection, or any other state.
enum class CursorIntent
{
    Default,          // no selection / nothing special: system default arrow
    Move,             // plain move order (ground or friendly/harmless target)
    Attack,           // hovering an enemy unit: move-into-contact (auto-acquires)
    Repair,           // selected Engineer hovering a CanRepairTarget patient
    InvalidPlacement, // placement mode over a CanPlaceBuilding-rejected tile
};

// selected == nullptr (empty selection) always yields Default. A hit enemy
// unit wins over repair (an Engineer hovering a damaged enemy is attack,
// not repair — CanRepairTarget already rejects cross-team patients, but the
// explicit team check documents the priority).
// Takes Registry by mutable reference only because PickUnitAt does; the
// prediction itself never mutates anything.
CursorIntent PredictCursorIntent(Registry &registry, const Unit *selected, Vector2 worldPos);
