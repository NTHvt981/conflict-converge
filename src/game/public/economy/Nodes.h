#pragma once

#include <functional>
#include <vector>

#include "MathUtils.h"
#include "Registry.h"
#include "ResourceSystem.h"
#include "Subsystem.h"

class TileMap;

// Resource node gathering: nodes sit on passable Grass tiles (gatherers stand
// on them), deplete as Engineers draw, and refill after a delay.

enum class ResourceKind
{
    Iron,
    Oil,
    Count
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

class ResourceNodes : public Subsystem
{
public:
    // Place a node on a Grass tile (one per tile); false leaves storage untouched.
    bool SpawnNode(const TileMap &map, ResourceKind kind, cc::IVec2 tile, float amount,
                   float respawnDelay);
    // Count down depleted nodes; refill when their timer expires.
    void Update(float dt);
    // Engineers standing on live nodes transfer to resources (teamID < 0 = all).
    void GatherTick(const Registry &registry, ResourceSystem &resources, float dt, int teamID = -1);

    std::size_t Count() const;
    const ResourceNode *FindAt(cc::IVec2 tile) const;
    // Read-only iteration (rendering, HUD).
    void Each(const std::function<void(const ResourceNode &)> &fn) const;

    // Save/load: restore a node verbatim (loader already validated the map).
    void RestoreNode(ResourceKind kind, cc::IVec2 tile, float amount, float maxAmount,
                     float respawnDelay, float respawnTimer);
    float IronCarry() const;
    float OilCarry() const;
    void SetCarry(float ironCarry, float oilCarry);

private:
    std::vector<ResourceNode> nodes_;
    float ironCarry_ = 0.0f; // fractional gather banked across ticks
    float oilCarry_ = 0.0f;
};
