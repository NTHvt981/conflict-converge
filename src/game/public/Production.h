#pragma once

#include <cstddef>
#include <vector>

#include "Unit.h" // UnitType
#include "raylib.h" // Vector2 rally point

class ResourceSystem; // fwd-decl (Production.cpp includes ResourceSystem.h)
class UnitFactory;    // fwd-decl (Production.cpp includes UnitFactory.h)

// M5 Goal 6: unit production queue. Costs are charged upfront at Enqueue
// (Command & Conquer style), so completion spawns prepaid: pass a UnitFactory
// to Update and finished items roll out at the rally point with full events.

// Placeholder build times (seconds); M5 balance pass tunes these.
float BuildTime(UnitType type);

class ProductionQueue
{
public:
    // Charge CostOf(type) via TrySpend and queue the build. False (nothing
    // queued, nothing spent) when funds are short.
    bool Enqueue(ResourceSystem &resources, UnitType type);
    // Drop the head item, refunding its full cost. No-op when empty.
    void CancelTop(ResourceSystem &resources);
    // Advance the head build; finished units SpawnPrepaid at rallyPos.
    void Update(UnitFactory &factory, int teamID, Vector2 rallyPos, float dt);

    bool Empty() const;
    std::size_t Size() const;
    float HeadProgress() const; // 0..1, 0 when empty

private:
    struct Item
    {
        UnitType type = UnitType::Infantry;
        float progress = 0.0f;
        float buildTime = 1.0f;
    };
    std::vector<Item> items_;
};
