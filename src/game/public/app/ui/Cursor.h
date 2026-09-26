#pragma once

#include "core/Registry.h"

#include "raylib.h"

struct Unit;

// Context-sensitive mouse cursors: what a right-click WOULD do if issued over
// worldPos right now. Pure prediction — never changes state.
enum class CursorIntent
{
    Default,          // no selection / nothing special: system default arrow
    Move,             // plain move order
    Attack,           // hovering an enemy unit (selected can attack)
    Repair,           // selected Engineer hovering a CanRepairTarget patient
    Heal,             // selected Medic hovering a CanHealTarget patient
    Load,             // selected carrier hovering a CanLoadTarget passenger
    InvalidPlacement, // placement mode over a CanPlaceBuilding-rejected tile
};

// selected == nullptr always yields Default; a hit enemy wins over repair.
// Takes Registry by mutable reference only because PickUnitAt does.
CursorIntent PredictCursorIntent(Registry &registry, const Unit *selected, Vector2 worldPos);
