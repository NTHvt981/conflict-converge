#pragma once

#include <functional>
#include <vector>

#include "MathUtils.h" // cc::IVec2
#include "Registry.h"  // Entity iteration for gatherers
#include "ResourceSystem.h" // gather destination

class TileMap; // fwd-decl (Nodes.cpp includes TileMap.h)

// M5 Goal 3: resource node gathering (Company of Heroes style: units work
// nodes in the field). Nodes sit on passable Grass tiles (gatherers stand on
// them), deplete as Engineers draw from them, and refill after a delay.

enum class ResourceKind
{
    Iron,
    Oil
};

struct ResourceNode
{
    ResourceKind kind = ResourceKind::Iron;
    cc::IVec2 tile{ 0, 0 };
    float amount = 0.0f;
    float maxAmount = 0.0f;
    float respawnDelay = 0.0f; // seconds from depletion to refill
    float respawnTimer = 0.0f; // live countdown while depleted

    bool IsDepleted() const;
};

class ResourceNodes
{
public:
    // Place a node on a Grass tile (one node per tile). Returns false for
    // water/building/out-of-bounds/occupied tiles, leaving storage untouched.
    bool SpawnNode(const TileMap &map, ResourceKind kind, cc::IVec2 tile, float amount,
                   float respawnDelay);
    // Count down depleted nodes; refill when their timer expires.
    void Update(float dt);
    // Engineers standing on live nodes transfer to resources (whole units
    // banked, fractions carried). Non-Engineers and the dead never gather.
    void GatherTick(const Registry &registry, ResourceSystem &resources, float dt);

    std::size_t Count() const;
    const ResourceNode *FindAt(cc::IVec2 tile) const;
    // Read-only iteration (rendering, HUD).
    void Each(const std::function<void(const ResourceNode &)> &fn) const;

private:
    std::vector<ResourceNode> nodes_;
    float ironCarry_ = 0.0f; // fractional gather banked across ticks
    float oilCarry_ = 0.0f;
};
