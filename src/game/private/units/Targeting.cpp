#include "Targeting.h"

#include "Building.h"  // M13: structural targets for raze orders
#include "FogOfWar.h"  // M9 visibility gate (nullptr = ungated, legacy tests)
#include "MathUtils.h" // glm distance via cc::Vec2

float DistanceBetween(const Unit &a, const Unit &b)
{
    return glm::distance(cc::ToGlm(a.position), cc::ToGlm(b.position));
}

Entity AcquireTarget(const Registry &registry, Entity seeker, const FogOfWar *fog)
{
    const Unit *self = registry.Get<Unit>(seeker);
    if (self == nullptr || self->sightRange <= 0.0f)
    {
        return kInvalidEntity;
    }
    // Artillery blind-fires into shroud at no penalty (Q78): exempt from fog.
    const bool seesThroughFog = (fog == nullptr) || self->type == UnitType::Artillery;

    Entity best = kInvalidEntity;
    int bestThreat = -1;
    float bestDist = 0.0f;
    registry.Each<Unit>([&](Entity candidate, const Unit &unit) {
        if (candidate == seeker || unit.teamID == self->teamID || unit.health <= 0.0f)
        {
            return; // self, ally, or already dead
        }
        const float dist = glm::distance(cc::ToGlm(self->position), cc::ToGlm(unit.position));
        if (dist > self->sightRange)
        {
            return;
        }
        if (!seesThroughFog &&
            !fog->IsVisible(self->teamID, cc::WorldToTile(cc::ToGlm(unit.position))))
        {
            return; // shrouded: hold fire until recon reveals the tile
        }
        if (unit.attackPower > bestThreat || (unit.attackPower == bestThreat && dist < bestDist))
        {
            best = candidate;
            bestThreat = unit.attackPower;
            bestDist = dist;
        }
    });
    return best;
}

bool InAttackRange(const Unit &attacker, const Unit &target)
{
    return DistanceBetween(attacker, target) <= static_cast<float>(attacker.attackRange);
}

Vector2 BuildingCenter(const Building &building)
{
    const cc::IVec2 size = Footprint(building.type);
    const Vector2 corner = cc::ToRaylib(cc::TileToWorld(building.tileX, building.tileY));
    return { corner.x + static_cast<float>(size.x) * cc::TILE_SIZE / 2.0f,
             corner.y + static_cast<float>(size.y) * cc::TILE_SIZE / 2.0f };
}

Entity AcquireBuildingTarget(const Registry &registry, Entity seeker, const FogOfWar *fog)
{
    const Unit *self = registry.Get<Unit>(seeker);
    if (self == nullptr || self->sightRange <= 0.0f)
    {
        return kInvalidEntity;
    }
    const bool seesThroughFog = (fog == nullptr) || self->type == UnitType::Artillery;

    Entity best = kInvalidEntity;
    float bestDist = 0.0f;
    registry.Each<Building>([&](Entity candidate, const Building &building) {
        if (building.teamID == self->teamID || building.state != BuildingState::Operational)
        {
            return; // own or already wrecked
        }
        const Vector2 center = BuildingCenter(building);
        const float dist = glm::distance(cc::ToGlm(self->position), cc::ToGlm(center));
        if (dist > self->sightRange)
        {
            return;
        }
        if (!seesThroughFog &&
            !fog->IsVisible(self->teamID, cc::WorldToTile(cc::ToGlm(center))))
        {
            return;
        }
        if (best == kInvalidEntity || dist < bestDist)
        {
            best = candidate;
            bestDist = dist;
        }
    });
    return best;
}
