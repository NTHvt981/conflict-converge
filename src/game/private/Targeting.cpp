#include "Targeting.h"

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
