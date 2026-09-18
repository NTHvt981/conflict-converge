#include "Building.h"

#include <cmath> // floor/ceil for repair quanta

#include "Nodes.h"
#include "TileMap.h"

// Placeholder base trickle (per Operational Base); balance pass tunes these.
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
    // Pacing tune: demolition must end games faster than production
    // replaces. A LightTank levels a Base in ~20s now.
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

Rectangle BuildingFootprintRect(const Building &building)
{
    const cc::IVec2 fp = Footprint(building.type);
    const cc::Vec2 corner = cc::TileToWorld(building.tileX, building.tileY);
    return { corner.x, corner.y, static_cast<float>(fp.x) * cc::TILE_SIZE,
             static_cast<float>(fp.y) * cc::TILE_SIZE };
}

void QueryBuildingsInRect(Registry &registry, Rectangle area, int teamID,
                          std::vector<Entity> &out)
{
    registry.Each<Building>([&](Entity id, const Building &building) {
        if (teamID >= 0 && building.teamID != teamID)
        {
            return;
        }
        if (CheckCollisionRecs(BuildingFootprintRect(building), area))
        {
            out.push_back(id);
        }
    });
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

bool CanPlaceBuilding(const TileMap &map, const ResourceNodes *nodes, BuildingType type,
                      int tileX, int tileY)
{
    const cc::IVec2 size = Footprint(type);
    if (!FootprintBuildable(map, size, tileX, tileY))
    {
        return false;
    }
    if (nodes != nullptr)
    {
        for (int y = 0; y < size.y; ++y)
        {
            for (int x = 0; x < size.x; ++x)
            {
                if (nodes->FindAt({ tileX + x, tileY + y }) != nullptr)
                {
                    return false;
                }
            }
        }
    }
    return true;
}

std::vector<cc::IVec2> AreaBuildSlots(BuildingType type, cc::IVec2 minTile,
                                      cc::IVec2 maxTile)
{
    std::vector<cc::IVec2> slots;
    const cc::IVec2 fp = Footprint(type);
    if (fp.x <= 0 || fp.y <= 0)
    {
        return slots;
    }
    for (int y = minTile.y; y <= maxTile.y; y += fp.y)
    {
        for (int x = minTile.x; x <= maxTile.x; x += fp.x)
        {
            slots.push_back({ x, y });
        }
    }
    return slots;
}

Entity PlaceBuilding(Registry &registry, TileMap &map, BuildingType type, int teamID, int tileX,
                     int tileY)
{
    return PlaceBuilding(registry, map, type, teamID, tileX, tileY, nullptr);
}

Entity PlaceBuilding(Registry &registry, TileMap &map, BuildingType type, int teamID, int tileX,
                     int tileY, const ResourceNodes *nodes)
{
    if (!CanPlaceBuilding(map, nodes, type, tileX, tileY))
    {
        return kInvalidEntity;
    }
    const cc::IVec2 size = Footprint(type);
    Building building;
    building.type = type;
    building.state = BuildingState::UnderConstruction; // ramps via UpdateBuildingConstruction
    building.teamID = teamID;
    building.tileX = tileX;
    building.tileY = tileY;
    building.health = 0.0f; // construction ramps 0 -> max (reads as "being built")
    building.maxHealth = BuildingMaxHealth(type);

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

// QoL auto-repair pacing: matches the Engineer channel rate so the paid
// convenience doesn't out-heal the free manual option; iron-only (placing
// structures is free, so there is no per-type cost table to mirror).
inline constexpr float kAutoRepairRateHPPerSec = 15.0f;
inline constexpr float kAutoRepairIronPerHP = 0.5f;

void UpdateBuildingAutoRepair(Registry &registry, ResourceSystem &resources, float dt, int teamID,
                              float capFraction)
{
    if (dt <= 0.0f || capFraction <= 0.0f)
    {
        return;
    }
    registry.Each<Building>([&](Entity, Building &building) {
        if (building.state != BuildingState::Operational ||
            (teamID >= 0 && building.teamID != teamID))
        {
            return;
        }
        const float missing = building.maxHealth - building.health;
        if (missing <= 0.0f)
        {
            building.repairCarry = 0.0f;
            return;
        }
        building.repairCarry += kAutoRepairRateHPPerSec * capFraction * dt;
        if (building.repairCarry > missing)
        {
            building.repairCarry = missing; // never bank past full
        }
        const float take = std::floor(std::min(building.repairCarry, missing));
        if (take < 1.0f)
        {
            return; // sub-HP remainder waits for more damage/budget
        }
        const long cost = static_cast<long>(std::ceil(take * kAutoRepairIronPerHP));
        if (!resources.TrySpend(cost, 0))
        {
            // Broke: pause with no progress banked (carry reset, never
            // partially charged) — poverty must not buy a burst heal later.
            building.repairCarry = 0.0f;
            return;
        }
        building.health += take;
        building.repairCarry -= take;
    });
}

float BuildingBuildTime(BuildingType type)
{
    switch (type)
    {
    case BuildingType::Base:
        return 3.0f;
    case BuildingType::ResourceDepot:
        return 2.0f;
    case BuildingType::Factory:
        return 4.0f;
    case BuildingType::Count:
        return 3.0f;
    }
    return 3.0f;
}

void UpdateBuildingConstruction(Registry &registry, float dt)
{
    if (dt <= 0.0f)
    {
        return;
    }
    registry.Each<Building>([&](Entity, Building &building) {
        if (building.state != BuildingState::UnderConstruction)
        {
            return;
        }
        building.constructionTime += dt;
        const float total = BuildingBuildTime(building.type);
        if (building.constructionTime >= total)
        {
            building.state = BuildingState::Operational;
            building.health = building.maxHealth; // exact, never overshoots
            return;
        }
        building.health = building.maxHealth * (building.constructionTime / total);
    });
}

// Entrance tiles (walkable tiles just outside the building footprint).
std::vector<cc::IVec2> BuildingEntrances(const TileMap &map, int tileX, int tileY,
                                          int fpW, int fpH)
{
    std::vector<cc::IVec2> result;
    // Scan every tile in the one-ring around the footprint.
    for (int y = tileY - 1; y <= tileY + fpH; ++y)
    {
        for (int x = tileX - 1; x <= tileX + fpW; ++x)
        {
            // Skip tiles inside the footprint itself.
            if (x >= tileX && x < tileX + fpW && y >= tileY && y < tileY + fpH)
            {
                continue;
            }
            if (!map.InBounds({ x, y }))
            {
                continue;
            }
            if (map.IsBlocked({ x, y }))
            {
                continue;
            }
            result.push_back({ x, y });
        }
    }
    return result;
}

// Attack positions around the building perimeter.
// Returns up to maxPositions evenly-spaced walkable tiles.
std::vector<cc::IVec2> BuildingAttackPositions(const TileMap &map, int tileX, int tileY,
                                                int fpW, int fpH, int maxPositions)
{
    // Collect all walkable perimeter tiles (same set as entrances).
    auto all = BuildingEntrances(map, tileX, tileY, fpW, fpH);
    if (all.empty() || static_cast<int>(all.size()) <= maxPositions)
    {
        return all;
    }
    // Evenly sample from the perimeter list.
    std::vector<cc::IVec2> result;
    result.reserve(maxPositions);
    const float step = static_cast<float>(all.size()) / static_cast<float>(maxPositions);
    for (int i = 0; i < maxPositions; ++i)
    {
        const int idx = static_cast<int>(static_cast<float>(i) * step);
        result.push_back(all[idx]);
    }
    return result;
}
