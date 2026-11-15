#pragma once

#include "core/Registry.h"

#include "raylib.h"

struct Unit;

enum class CursorIntent
{
	Default,
	Move,
	Attack,
	Repair,
	Heal,
	Load,
	InvalidPlacement,
};

// selected == nullptr always yields Default; a hit enemy wins over repair.
CursorIntent PredictCursorIntent(Registry &registry, const Unit *selected, Vector2 worldPos);
