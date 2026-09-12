#include "Building.h"

#include "TileMap.h"

// Placeholder base trickle (per Operational Base); M5 balance pass tunes these.
inline constexpr float kBaseIronPerSecond = 2.0f;
inline constexpr float kBaseOilPerSecond = 1.0f;

cc::IVec2 Footprint(BuildingType type)
{
    switch (type)
    {
    case BuildingType::Base:
        return { 2, 2 };
    case BuildingType::ResourceDepot:
        return { 1, 1 };
    case BuildingType::Factory:
        return { 2, 2 };
    }
    return { 1, 1 };
}

float BuildingMaxHealth(BuildingType type)
{
    // M13 pacing tune: demolition must end games faster than production
    // replaces (soak evidence). A LightTank levels a Base in ~20s now.
    switch (type)
    {
    case BuildingType::Base:
        return 400.0f;
    case BuildingType::ResourceDepot:
        return 200.0f;
    case BuildingType::Factory:
        return 350.0f;
    }
    return 200.0f;
}

namespace
{

// Every footprint tile must exist and be buildable Grass. Placed buildings
// mark their tiles, so overlap and water/edge cases fall out of one check.
// (Units standing on the site are allowed: no unit collision system yet.)
bool FootprintBuildable(const TileMap &map, cc::IVec2 size, int tileX, int tileY)
{
    for (int y = 0; y < size.y; ++y)
    {
        for (int x = 0; x < size.x; ++x)
        {
            const cc::IVec2 tile{ tileX + x, tileY + y };
            if (!map.InBounds(tile) || map.Get(tile) != TerrainType::Grass)
            {
                return false;
            }
        }
    }
    return true;
}

void MarkFootprint(TileMap &map, cc::IVec2 size, int tileX, int tileY, TerrainType terrain)
{
    for (int y = 0; y < size.y; ++y)
    {
        for (int x = 0; x < size.x; ++x)
        {
            map.Set({ tileX + x, tileY + y }, terrain);
        }
    }
}

} // namespace

Entity PlaceBuilding(Registry &registry, TileMap &map, BuildingType type, int teamID, int tileX,
                     int tileY)
{
    const cc::IVec2 size = Footprint(type);
    if (!FootprintBuildable(map, size, tileX, tileY))
    {
        return kInvalidEntity;
    }
    Building building;
    building.type = type;
    building.state = BuildingState::Operational;
    building.teamID = teamID;
    building.tileX = tileX;
    building.tileY = tileY;
    building.health = BuildingMaxHealth(type);
    building.maxHealth = building.health;

    const Entity id = registry.Create();
    registry.Add(id, building);
    MarkFootprint(map, size, tileX, tileY, TerrainType::Building);
    return id;
}

bool DemolishBuilding(Registry &registry, TileMap &map, Entity entity)
{
    Building *building = registry.Get<Building>(entity);
    if (building == nullptr)
    {
        return false;
    }
    building->state = BuildingState::Destroyed;
    MarkFootprint(map, Footprint(building->type), building->tileX, building->tileY,
                  TerrainType::Grass);
    registry.Destroy(entity);
    return true;
}

void UpdateBaseIncome(const Registry &registry, ResourceSystem &resources, float dt, int teamID)
{
    int bases = 0;
    registry.Each<Building>([&](Entity, const Building &building) {
        if (building.state == BuildingState::Operational && building.type == BuildingType::Base &&
            (teamID < 0 || building.teamID == teamID))
        {
            ++bases;
        }
    });
    if (bases > 0)
    {
        resources.TickIncome(kBaseIronPerSecond * static_cast<float>(bases),
                             kBaseOilPerSecond * static_cast<float>(bases), dt);
    }
}
