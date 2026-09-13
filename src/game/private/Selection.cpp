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

Rectangle NormalizeRect(Vector2 a, Vector2 b)
{
    Rectangle box;
    box.x = a.x < b.x ? a.x : b.x;
    box.y = a.y < b.y ? a.y : b.y;
    box.width = a.x < b.x ? b.x - a.x : a.x - b.x;
    box.height = a.y < b.y ? b.y - a.y : a.y - b.y;
    return box;
}

int SelectInRect(Registry &registry, Rectangle worldBox, bool add)
{
    const Rectangle box = NormalizeRect({ worldBox.x, worldBox.y },
                                        { worldBox.x + worldBox.width,
                                          worldBox.y + worldBox.height });
    int picked = 0;
    registry.Each<Unit>([&](Entity, Unit &unit) {
        // Body center (32x32 hitbox inset, matches the M4 selection visual).
        const float cx = unit.position.x + 32.0f;
        const float cy = unit.position.y + 32.0f;
        const bool inside = cx >= box.x && cx <= box.x + box.width && cy >= box.y &&
                            cy <= box.y + box.height;
        if (!add)
        {
            unit.isSelected = inside;
        }
        else if (inside)
        {
            unit.isSelected = true;
        }
        if (unit.isSelected && inside)
        {
            ++picked;
        }
    });
    return picked;
}
