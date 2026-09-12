#include "Nodes.h"

#include "TileMap.h"
#include "Unit.h" // Engineer gatherers

// Placeholder harvest rates (units/second); M5 balance pass tunes these.
inline constexpr float kGatherIronPerSecond = 10.0f;
inline constexpr float kGatherOilPerSecond = 8.0f;

bool ResourceNode::IsDepleted() const
{
    return amount <= 0.0f;
}

bool ResourceNodes::SpawnNode(const TileMap &map, ResourceKind kind, cc::IVec2 tile,
                              float amount, float respawnDelay)
{
    if (amount <= 0.0f || respawnDelay < 0.0f || !map.InBounds(tile) ||
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
        if (!node.IsDepleted())
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

void ResourceNodes::GatherTick(const Registry &registry, ResourceSystem &resources, float dt)
{
    if (dt <= 0.0f)
    {
        return;
    }
    registry.Each<Unit>([&](Entity, const Unit &unit) {
        if (unit.type != UnitType::Engineer || unit.health <= 0.0f)
        {
            return; // only living Engineers work nodes
        }
        const cc::IVec2 tile = cc::WorldToTile(cc::ToGlm(unit.position));
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
                break; // one node per tile
            }
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
