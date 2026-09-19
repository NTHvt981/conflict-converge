#pragma once

#include <cstddef>
#include <vector>

#include "Unit.h"
#include "raylib.h"

class ResourceSystem;
class UnitFactory;

// Unit production queue: costs charged upfront at Enqueue, so completion
// spawns prepaid through the UnitFactory at the rally point.

// Placeholder build times (seconds).
float BuildTime(UnitType type);

class ProductionQueue
{
public:
    // Charge CostOf(type) via TrySpend and queue the build.
    bool Enqueue(ResourceSystem &resources, UnitType type, bool repeat = false);
    // Drop the head item, refunding its full cost. No-op when empty.
    void CancelTop(ResourceSystem &resources);
    // Repeat-arming for the factory panel's per-type toggle.
    void SetRepeatArmed(UnitType type, bool repeat);
    bool RepeatArmed(UnitType type) const;
    // Advance the head build; finished units SpawnPrepaid at rallyPos.
    Entity Update(UnitFactory &factory, ResourceSystem &resources, int teamID, Vector2 rallyPos,
                  float dt);

    bool Empty() const;
    std::size_t Size() const;
    float HeadProgress() const; // 0..1, 0 when empty

private:
    struct Item
    {
        UnitType type = UnitType::Infantry;
        float progress = 0.0f;
        float buildTime = 1.0f;
        bool repeat = false;
    };
    static constexpr int kTypeCount = static_cast<int>(UnitType::Count);
    std::vector<Item> items_;
    bool repeatArmed_[kTypeCount] = {};
};
