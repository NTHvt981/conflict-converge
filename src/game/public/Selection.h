#pragma once

#include "raylib.h" // Vector2, Rectangle

#include "Registry.h" // Entity, kInvalidEntity

// M2 Goal 4: mouse selection (left-click pick) over the unit registry.
// Hit test is the unit's 64x64 tile box at its snapped position.

Entity PickUnitAt(Registry &registry, Vector2 worldPos);
void SelectOnly(Registry &registry, Entity entity);
void DeselectAll(Registry &registry);

// First selected unit, or kInvalidEntity (right-click orders target this).
Entity SelectedUnit(Registry &registry);

// Normalize a drag box (either corner may lead) to positive width/height.
Rectangle NormalizeRect(Vector2 a, Vector2 b);

// Box-select every unit whose body center falls inside worldBox. With
// add=false the box replaces the selection, otherwise it extends it.
// Returns how many units the box picked.
int SelectInRect(Registry &registry, Rectangle worldBox, bool add);
