#pragma once

#include "raylib.h" // Vector2

#include "Registry.h" // Entity, kInvalidEntity

// M2 Goal 4: mouse selection (left-click pick) over the unit registry.
// Hit test is the unit's 64x64 tile box at its snapped position.

Entity PickUnitAt(Registry &registry, Vector2 worldPos);
void SelectOnly(Registry &registry, Entity entity);
void DeselectAll(Registry &registry);

// First selected unit, or kInvalidEntity (right-click orders target this).
Entity SelectedUnit(Registry &registry);
