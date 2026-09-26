#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "core/Subsystem.h"
#include "units/Unit.h"
#include "units/UnitStats.h"
#include "raylib.h"

class ResourceSystem;
class UnitFactory;
class TileMap;
class OccupancyGrid;

// Build times in seconds.
float BuildTime(UnitType type);

class ProductionQueue : public Subsystem
{
public:
    explicit ProductionQueue(std::optional<ProductionCategory> category = std::nullopt);
    bool Enqueue(ResourceSystem &resources, UnitType type, bool repeat = false);
    // No-op when empty.
    void CancelTop(ResourceSystem &resources);
    void SetRepeatArmed(UnitType type, bool repeat);
    bool RepeatArmed(UnitType type) const;
    Entity Update(UnitFactory &factory, ResourceSystem &resources, int teamID, Vector2 spawnPos,
                  float dt, const TileMap *map = nullptr, const OccupancyGrid *occ = nullptr);

    bool Empty() const;
    std::size_t Size() const;
    float HeadProgress() const; // 0..1, 0 when empty
    std::optional<ProductionCategory> Category() const { return category_; }
    void ResetForMatch() override;

private:
    struct Item
    {
        UnitType type = UnitType::RifleInfantry;
        float progress = 0.0f;
        float buildTime = 1.0f;
        bool repeat = false;
    };
    static constexpr int kTypeCount = static_cast<int>(UnitType::Count);
    std::optional<ProductionCategory> category_;
    std::vector<Item> items_;
    bool repeatArmed_[kTypeCount] = {};
};
