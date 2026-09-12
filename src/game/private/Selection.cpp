#include "Selection.h"

#include "MathUtils.h" // cc::TILE_SIZE hit box
#include "Unit.h"

Entity PickUnitAt(Registry &registry, Vector2 worldPos)
{
    Entity found = kInvalidEntity;
    registry.Each<Unit>([&](Entity entity, Unit &unit) {
        if (found != kInvalidEntity)
        {
            return;
        }
        const bool insideX = worldPos.x >= unit.position.x && worldPos.x < unit.position.x + cc::TILE_SIZE;
        const bool insideY = worldPos.y >= unit.position.y && worldPos.y < unit.position.y + cc::TILE_SIZE;
        if (insideX && insideY)
        {
            found = entity;
        }
    });
    return found;
}

void SelectOnly(Registry &registry, Entity entity)
{
    registry.Each<Unit>([&](Entity current, Unit &unit) { unit.isSelected = (current == entity); });
}

void DeselectAll(Registry &registry)
{
    registry.Each<Unit>([&](Entity, Unit &unit) { unit.isSelected = false; });
}

Entity SelectedUnit(Registry &registry)
{
    Entity found = kInvalidEntity;
    registry.Each<Unit>([&](Entity entity, Unit &unit) {
        if (found == kInvalidEntity && unit.isSelected)
        {
            found = entity;
        }
    });
    return found;
}
