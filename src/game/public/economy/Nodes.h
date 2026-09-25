#pragma once

#include <functional>
#include <vector>

#include "MathUtils.h"
#include "Registry.h"
#include "ResourceSystem.h"
#include "Subsystem.h"

class TileMap;

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
    float respawnTimer = 0.0f;

    bool IsDepleted() const;
};

class ResourceNodes : public Subsystem
{
public:
    // False leaves storage untouched.
    bool SpawnNode(const TileMap &map, ResourceKind kind, cc::IVec2 tile, float amount,
                   float respawnDelay);
    void Update(float dt);
    // teamID < 0 = all teams.
    void GatherTick(const Registry &registry, ResourceSystem &resources, float dt, int teamID = -1);

    std::size_t Count() const;
    const ResourceNode *FindAt(cc::IVec2 tile) const;
    void Each(const std::function<void(const ResourceNode &)> &fn) const;

    // Save/load: restore a node verbatim (loader already validated the map).
    void RestoreNode(ResourceKind kind, cc::IVec2 tile, float amount, float maxAmount,
                     float respawnDelay, float respawnTimer);
    float IronCarry() const;
    float OilCarry() const;
    void SetCarry(float ironCarry, float oilCarry);

private:
    std::vector<ResourceNode> nodes_;
    float ironCarry_ = 0.0f;
    float oilCarry_ = 0.0f;
};
