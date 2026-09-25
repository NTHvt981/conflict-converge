#include "Selection.h"

#include "Building.h"
#include "Extensions.h"
#include "MathUtils.h"
#include "Unit.h"

Entity PickUnitAt(Registry &registry, Vector2 worldPos)
{
    Entity found = kInvalidEntity;
    registry.Each<Unit>([&](Entity entity, Unit &unit) {
        if (found != kInvalidEntity || IsEmbarked(registry, entity))
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

int SelectedUnitCount(Registry &registry)
{
    int count = 0;
    registry.Each<Unit>([&](Entity, const Unit &unit) {
        if (unit.isSelected)
        {
            ++count;
        }
    });
    return count;
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

Rectangle DraggedWorldBox(const GameCamera &camera, Vector2 screenStart, Vector2 screenEnd)
{
    return NormalizeRect(camera.ScreenToWorld(screenStart), camera.ScreenToWorld(screenEnd));
}

std::pair<Vector2, Vector2> DraggedWorldLine(const GameCamera &camera, Vector2 screenStart,
                                             Vector2 screenEnd)
{
    return { camera.ScreenToWorld(screenStart), camera.ScreenToWorld(screenEnd) };
}

int SelectInRect(Registry &registry, Rectangle worldBox, bool add)
{
    const Rectangle box = NormalizeRect({ worldBox.x, worldBox.y },
                                        { worldBox.x + worldBox.width,
                                          worldBox.y + worldBox.height });
    int picked = 0;
    registry.Each<Unit>([&](Entity id, Unit &unit) {
        if (IsEmbarked(registry, id))
        {
            unit.isSelected = false;
            return;
        }
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

namespace
{

bool ValidGroupBit(int groupBit)
{
    return groupBit >= 0 && groupBit < 10;
}

unsigned int GroupMask(int groupBit)
{
    return 1u << static_cast<unsigned int>(groupBit);
}

}

int AssignControlGroup(Registry &registry, int groupBit)
{
    if (!ValidGroupBit(groupBit))
    {
        return 0;
    }
    const unsigned int mask = GroupMask(groupBit);
    int stamped = 0;
    registry.Each<Unit>([&](Entity, Unit &unit) {
        if (unit.isSelected)
        {
            unit.controlGroups |= mask;
            ++stamped;
        }
        else
        {
            unit.controlGroups &= ~mask;
        }
    });
    return stamped;
}

int AddToControlGroup(Registry &registry, int groupBit)
{
    if (!ValidGroupBit(groupBit))
    {
        return 0;
    }
    const unsigned int mask = GroupMask(groupBit);
    int stamped = 0;
    registry.Each<Unit>([&](Entity, Unit &unit) {
        if (unit.isSelected)
        {
            unit.controlGroups |= mask;
            ++stamped;
        }
    });
    return stamped;
}

int RecallControlGroup(Registry &registry, int groupBit)
{
    if (!ValidGroupBit(groupBit))
    {
        return 0;
    }
    const unsigned int mask = GroupMask(groupBit);
    int picked = 0;
    registry.Each<Unit>([&](Entity, Unit &unit) {
        unit.isSelected = (unit.controlGroups & mask) != 0;
        if (unit.isSelected)
        {
            ++picked;
        }
    });
    return picked;
}

namespace
{

bool IsIdleWorker(const Unit &unit, const Orders *orders, const Mover *mover, int teamID,
                  bool workersOnly)
{
    if (unit.teamID != teamID || unit.state != UnitState::Idle ||
        (mover != nullptr && (mover->hasMoveOrder || mover->hasPath)) ||
        (orders != nullptr && orders->hasRepairOrder))
    {
        return false;
    }
    const bool isEngineer = unit.type == UnitType::Engineer;
    return workersOnly ? isEngineer : !isEngineer;
}

}

int SelectIdle(Registry &registry, int teamID, bool workersOnly)
{
    int picked = 0;
    registry.Each<Unit>([&](Entity id, Unit &unit) {
        unit.isSelected =
            IsIdleWorker(unit, FindOrders(registry, id), FindMover(registry, id), teamID,
                         workersOnly);
        if (unit.isSelected)
        {
            ++picked;
        }
    });
    return picked;
}

int CountIdle(const Registry &registry, int teamID, bool workersOnly)
{
    int count = 0;
    registry.Each<Unit>([&](Entity id, const Unit &unit) {
        if (IsIdleWorker(unit, FindOrders(registry, id), FindMover(registry, id), teamID,
                         workersOnly))
        {
            ++count;
        }
    });
    return count;
}

int SelectAllOfType(Registry &registry, UnitType type, int teamID, bool add)
{
    int picked = 0;
    registry.Each<Unit>([&](Entity, Unit &unit) {
        const bool match = unit.teamID == teamID && unit.type == type;
        if (!add)
        {
            unit.isSelected = match;
        }
        else if (match)
        {
            unit.isSelected = true;
        }
        if (unit.isSelected && match)
        {
            ++picked;
        }
    });
    return picked;
}

int SelectAllOfTypeInRect(Registry &registry, Rectangle worldViewport, UnitType type,
                          int teamID, bool add)
{
    const Rectangle box = NormalizeRect({ worldViewport.x, worldViewport.y },
                                        { worldViewport.x + worldViewport.width,
                                          worldViewport.y + worldViewport.height });
    int picked = 0;
    registry.Each<Unit>([&](Entity, Unit &unit) {
        const float cx = unit.position.x + 32.0f;
        const float cy = unit.position.y + 32.0f;
        const bool inside = cx >= box.x && cx <= box.x + box.width && cy >= box.y &&
                            cy <= box.y + box.height;
        const bool match = inside && unit.teamID == teamID && unit.type == type;
        if (!add)
        {
            unit.isSelected = match;
        }
        else if (match)
        {
            unit.isSelected = true;
        }
        if (unit.isSelected && match)
        {
            ++picked;
        }
    });
    return picked;
}

int SelectAllBuildings(Registry &registry, BuildingType type, int teamID)
{
    int picked = 0;
    registry.Each<Building>([&](Entity, Building &building) {
        building.isSelected = (building.teamID == teamID && building.type == type);
        if (building.isSelected)
        {
            ++picked;
        }
    });
    return picked;
}
