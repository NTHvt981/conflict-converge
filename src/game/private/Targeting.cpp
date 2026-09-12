#include "Targeting.h"

#include "MathUtils.h" // glm distance via cc::Vec2

float DistanceBetween(const Unit &a, const Unit &b)
{
    return glm::distance(cc::ToGlm(a.position), cc::ToGlm(b.position));
}

Entity AcquireTarget(const Registry &registry, Entity seeker)
{
    const Unit *self = registry.Get<Unit>(seeker);
    if (self == nullptr || self->sightRange <= 0.0f)
    {
        return kInvalidEntity;
    }

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
