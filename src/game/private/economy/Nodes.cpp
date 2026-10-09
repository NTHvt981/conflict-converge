#include "economy/Nodes.h"

#include "economy/Building.h"
#include "units/Extensions.h"
#include "world/Pathfinder.h"
#include "world/TileMap.h"
#include "units/Unit.h"

inline constexpr float kGatherIronPerSecond = 10.0f;
inline constexpr float kGatherOilPerSecond = 8.0f;
inline constexpr float kLoadIronPerSecond = 16.0f;

namespace
{

bool OnDepotEntrance(const Registry &registry, cc::IVec2 tile, int teamID)
{
    bool found = false;
    registry.Each<Building>([&](Entity, const Building &building) {
        if (found || building.teamID != teamID ||
            building.state != BuildingState::Operational ||
            building.type != BuildingType::ResourceDepot)
        {
            return;
        }
        const cc::IVec2 fp = Footprint(building.type);
        for (int dy = -1; dy <= fp.y && !found; ++dy)
        {
            for (int dx = -1; dx <= fp.x && !found; ++dx)
            {
                if (dx >= 0 && dx < fp.x && dy >= 0 && dy < fp.y)
                {
                    continue;
                }
                if (tile == cc::IVec2(building.tileX + dx, building.tileY + dy))
                {
                    found = true;
                }
            }
        }
    });
    return found;
}

cc::IVec2 NearestDepotEntrance(const Registry &registry, const TileMap &map, cc::IVec2 from,
                               int teamID)
{
    cc::IVec2 best{ -1, -1 };
    int bestDist = 0;
    bool have = false;
    registry.Each<Building>([&](Entity, const Building &building) {
        if (building.teamID != teamID || building.state != BuildingState::Operational ||
            building.type != BuildingType::ResourceDepot)
        {
            return;
        }
        const cc::IVec2 fp = Footprint(building.type);
        for (int dy = -1; dy <= fp.y; ++dy)
        {
            for (int dx = -1; dx <= fp.x; ++dx)
            {
                if (dx >= 0 && dx < fp.x && dy >= 0 && dy < fp.y)
                {
                    continue;
                }
                const cc::IVec2 tile{ building.tileX + dx, building.tileY + dy };
                if (!map.InBounds(tile) || map.IsBlocked(tile))
                {
                    continue;
                }
                const int dist = (tile.x - from.x) * (tile.x - from.x) +
                                 (tile.y - from.y) * (tile.y - from.y);
                if (!have || dist < bestDist)
                {
                    have = true;
                    bestDist = dist;
                    best = tile;
                }
            }
        }
    });
    return best;
}

void OrderHarvestMove(Registry &registry, Unit &unit, Entity id, const TileMap &map,
                      const OccupancyGrid *occ, Vector2 dest)
{
    if (occ != nullptr)
    {
        IssuePathOrderFootprint(unit, GetOrders(registry, id), GetMover(registry, id), map, *occ,
                                dest, id, registry.Generation(id));
    }
    else
    {
        IssuePathOrder(unit, GetOrders(registry, id), GetMover(registry, id), map, dest);
    }
}

} // namespace

bool ResourceNode::IsDepleted() const
{
    return amount <= 0.0f;
}

bool ResourceNodes::SpawnNode(const TileMap &map, ResourceKind kind, cc::IVec2 tile,
                               float amount, float respawnDelay)
{
    if (amount <= 0.0f || !map.InBounds(tile) ||
        map.Get(tile) != TerrainType::Grass || FindAt(tile) != nullptr)
    {
        return false;
    }
    ResourceNode node;
    node.kind = kind;
    node.tile = tile;
    node.amount = amount;
    node.maxAmount = amount;
    node.respawnDelay = respawnDelay;
    node.respawnTimer = 0.0f;
    nodes_.push_back(node);
    return true;
}

void ResourceNodes::Update(float dt)
{
    if (dt <= 0.0f)
    {
        return;
    }
    for (ResourceNode &node : nodes_)
    {
        if (!node.IsDepleted() || node.respawnDelay < 0.0f)
        {
            continue;
        }
        node.respawnTimer -= dt;
        if (node.respawnTimer <= 0.0f)
        {
            node.amount = node.maxAmount;
            node.respawnTimer = 0.0f;
        }
    }
}

void ResourceNodes::GatherTick(Registry &registry, ResourceSystem &resources, float dt,
                               int teamID)
{
    if (dt <= 0.0f)
    {
        return;
    }
    registry.Each<Unit>([&](Entity id, const Unit &unit) {
        if (unit.type != UnitType::Engineer || unit.health <= 0.0f)
        {
            return;
        }
        if (teamID >= 0 && unit.teamID != teamID)
        {
            return;
        }
        const cc::IVec2 tile = cc::WorldToTile(cc::ToGlm(unit.position));
        if (Harvester *harvester = registry.Get<Harvester>(id))
        {
            if (harvester->phase == HarvestPhase::Loading && tile == harvester->nodeTile)
            {
                for (ResourceNode &node : nodes_)
                {
                    if (!(node.tile == tile) || node.IsDepleted())
                    {
                        continue;
                    }
                    const float rate = node.kind == ResourceKind::Iron ? kLoadIronPerSecond
                                                                       : kGatherOilPerSecond;
                    float take = rate * dt;
                    if (take > node.amount)
                    {
                        take = node.amount;
                    }
                    if (take > harvester->capacity - harvester->load)
                    {
                        take = harvester->capacity - harvester->load;
                    }
                    if (take <= 0.0f)
                    {
                        break;
                    }
                    node.amount -= take;
                    if (node.IsDepleted())
                    {
                        node.respawnTimer = node.respawnDelay;
                    }
                    harvester->load += take;
                    harvester->loadKind = static_cast<int>(node.kind);
                    if (harvester->load >= harvester->capacity)
                    {
                        harvester->phase = HarvestPhase::ToDepot;
                    }
                    break;
                }
            }
            else if (harvester->phase == HarvestPhase::Unloading &&
                     OnDepotEntrance(registry, tile, unit.teamID))
            {
                const long whole = static_cast<long>(harvester->load);
                long stored = 0;
                if (harvester->loadKind == static_cast<int>(ResourceKind::Oil))
                {
                    stored = DepositOil(registry, resources, unit.teamID, whole);
                }
                else
                {
                    stored = DepositIron(registry, resources, unit.teamID, whole);
                }
                harvester->load -= static_cast<float>(stored);
                if (harvester->load <= 0.0f)
                {
                    harvester->load = 0.0f;
                    harvester->phase = HarvestPhase::ToField;
                    harvester->nodeTile = { -1, -1 };
                }
            }
            return;
        }
        for (ResourceNode &node : nodes_)
        {
            if (node.tile == tile && !node.IsDepleted())
            {
                const float rate =
                    node.kind == ResourceKind::Iron ? kGatherIronPerSecond : kGatherOilPerSecond;
                float &carry = node.kind == ResourceKind::Iron ? ironCarry_ : oilCarry_;
                float take = rate * dt;
                if (take > node.amount)
                {
                    take = node.amount;
                }
                node.amount -= take;
                if (node.IsDepleted())
                {
                    node.respawnTimer = node.respawnDelay;
                }
                carry += take;
                long whole = static_cast<long>(carry);
                if (node.kind == ResourceKind::Iron)
                {
                    resources.AddIron(whole);
                }
                else
                {
                    resources.AddOil(whole);
                }
                carry -= static_cast<float>(whole);
                break;
            }
        }
    });
}

void ResourceNodes::UpdateHarvesters(Registry &registry, const TileMap &map,
                                     const OccupancyGrid *occ, float dt, int teamID)
{
    if (dt <= 0.0f)
    {
        return;
    }
    registry.Each<Unit>([&](Entity id, Unit &unit) {
        if (unit.type != UnitType::Engineer || unit.health <= 0.0f)
        {
            return;
        }
        if (teamID >= 0 && unit.teamID != teamID)
        {
            return;
        }
        Harvester *harvester = registry.Get<Harvester>(id);
        if (harvester == nullptr)
        {
            return;
        }
        const Mover *mover = FindMover(registry, id);
        const bool idle = mover == nullptr || (!mover->hasMoveOrder && !mover->hasPath);
        const cc::IVec2 here = cc::WorldToTile(cc::ToGlm(unit.position));
        switch (harvester->phase)
        {
        case HarvestPhase::ToField:
        {
            const ResourceNode *node = FindAt(harvester->nodeTile);
            if (here == harvester->nodeTile && node != nullptr && !node->IsDepleted())
            {
                harvester->phase = HarvestPhase::Loading;
                break;
            }
            if (node == nullptr || node->IsDepleted())
            {
                const ResourceNode *best = nullptr;
                int bestDist = 0;
                Each([&](const ResourceNode &candidate) {
                    if (candidate.IsDepleted())
                    {
                        return;
                    }
                    const int dist = (candidate.tile.x - here.x) * (candidate.tile.x - here.x) +
                                     (candidate.tile.y - here.y) * (candidate.tile.y - here.y);
                    if (best == nullptr || dist < bestDist)
                    {
                        best = &candidate;
                        bestDist = dist;
                    }
                });
                if (best == nullptr)
                {
                    break;
                }
                harvester->nodeTile = best->tile;
            }
            if (idle && !(here == harvester->nodeTile))
            {
                OrderHarvestMove(registry, unit, id, map, occ,
                                 cc::ToRaylib(cc::TileToWorld(harvester->nodeTile.x,
                                                              harvester->nodeTile.y)));
            }
            break;
        }
        case HarvestPhase::Loading:
        {
            if (harvester->load >= harvester->capacity)
            {
                harvester->phase = HarvestPhase::ToDepot;
            }
            else
            {
                const ResourceNode *node = FindAt(harvester->nodeTile);
                if (node == nullptr || node->IsDepleted() || !(here == harvester->nodeTile))
                {
                    harvester->phase = HarvestPhase::ToField;
                }
            }
            break;
        }
        case HarvestPhase::ToDepot:
            if (OnDepotEntrance(registry, here, unit.teamID))
            {
                harvester->phase = HarvestPhase::Unloading;
            }
            else if (idle)
            {
                const cc::IVec2 entrance = NearestDepotEntrance(registry, map, here, unit.teamID);
                if (entrance.x < 0)
                {
                    break;
                }
                OrderHarvestMove(registry, unit, id, map, occ,
                                 cc::ToRaylib(cc::TileToWorld(entrance.x, entrance.y)));
            }
            break;
        case HarvestPhase::Unloading:
            if (harvester->load <= 0.0f)
            {
                harvester->phase = HarvestPhase::ToField;
                harvester->nodeTile = { -1, -1 };
            }
            else if (!OnDepotEntrance(registry, here, unit.teamID))
            {
                harvester->phase = HarvestPhase::ToDepot;
            }
            break;
        }
    });
}

std::size_t ResourceNodes::Count() const
{
    return nodes_.size();
}

const ResourceNode *ResourceNodes::FindAt(cc::IVec2 tile) const
{
    for (const ResourceNode &node : nodes_)
    {
        if (node.tile == tile)
        {
            return &node;
        }
    }
    return nullptr;
}

void ResourceNodes::Each(const std::function<void(const ResourceNode &)> &fn) const
{
    for (const ResourceNode &node : nodes_)
    {
        fn(node);
    }
}

void ResourceNodes::RestoreNode(ResourceKind kind, cc::IVec2 tile, float amount, float maxAmount,
                                float respawnDelay, float respawnTimer)
{
    ResourceNode node;
    node.kind = kind;
    node.tile = tile;
    node.amount = amount;
    node.maxAmount = maxAmount;
    node.respawnDelay = respawnDelay;
    node.respawnTimer = respawnTimer;
    nodes_.push_back(node);
}

float ResourceNodes::IronCarry() const
{
    return ironCarry_;
}

float ResourceNodes::OilCarry() const
{
    return oilCarry_;
}

void ResourceNodes::SetCarry(float ironCarry, float oilCarry)
{
    ironCarry_ = ironCarry;
    oilCarry_ = oilCarry;
}
