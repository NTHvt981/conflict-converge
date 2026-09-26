
#include "units/Unit.h"

#include "units/UnitInternal.h"
#include "economy/Building.h"
#include "units/Combat.h"
#include "units/Extensions.h"
#include "core/MathUtils.h"
#include "units/Targeting.h"
#include "world/TileMap.h"
#include "units/UnitStats.h"

namespace
{

bool IsRepairableUnit(const Unit &unit)
{
    return unit.type == UnitType::IFV || unit.type == UnitType::Artillery ||
           unit.type == UnitType::LightTank || unit.type == UnitType::HeavyTank;
}

bool IsHealableUnit(const Unit &unit)
{
    return unit.type == UnitType::RifleInfantry || unit.type == UnitType::AntiArmorInfantry ||
           unit.type == UnitType::Engineer || unit.type == UnitType::PrototypeInfantry ||
           unit.type == UnitType::Medic;
}

Vector2 RepairCandidatePos(const Registry &registry, Entity candidate)
{
    if (const Unit *unit = registry.Get<Unit>(candidate))
    {
        return unit->position;
    }
    if (const Building *building = registry.Get<Building>(candidate))
    {
        return BuildingCenter(*building);
    }
    return { 0.0f, 0.0f };
}

constexpr float kAutoRepairAcquireRange = 256.0f;

}

bool RepairAim(const Registry &registry, const Unit &engineer, Entity target, Vector2 &outPos)
{
    if (engineer.type != UnitType::Engineer)
    {
        return false;
    }
    if (const Unit *u = registry.Get<Unit>(target))
    {
        if (u->health <= 0.0f || u->teamID != engineer.teamID || !IsRepairableUnit(*u) ||
            IsEmbarked(registry, target))
        {
            return false;
        }
        if (u->health >= BaseStats(u->type).health)
        {
            return false;
        }
        outPos = u->position;
        return true;
    }
    if (const Building *b = registry.Get<Building>(target))
    {
        if (b->state != BuildingState::Operational || b->teamID != engineer.teamID)
        {
            return false;
        }
        if (b->health >= b->maxHealth)
        {
            return false;
        }
        outPos = BuildingCenter(*b);
        return true;
    }
    return false;
}

bool CanRepairTarget(const Registry &registry, const Unit &engineer, Entity target)
{
    Vector2 aim = {};
    return RepairAim(registry, engineer, target, aim);
}

void AcquireAutoRepair(Registry &registry, Entity self, const Unit &engineer, Orders &orders)
{
    Entity best = kInvalidEntity;
    float bestDist = kAutoRepairAcquireRange;
    auto consider = [&](Entity candidate) {
        if (candidate == self || candidate == kInvalidEntity)
        {
            return;
        }
        Vector2 aim = {};
        if (!RepairAim(registry, engineer, candidate, aim))
        {
            return;
        }
        const float dist = glm::distance(cc::ToGlm(engineer.position), cc::ToGlm(aim));
        if (dist < bestDist)
        {
            bestDist = dist;
            best = candidate;
        }
    };
    registry.Each<Unit>([&](Entity id, const Unit &) { consider(id); });
    registry.Each<Building>([&](Entity id, const Building &) { consider(id); });
    if (best != kInvalidEntity)
    {
        orders.hasRepairOrder = true;
        orders.repairTarget = best;
    }
}

bool HealAim(const Registry &registry, const Unit &medic, Entity target, Vector2 &outPos)
{
    if (medic.type != UnitType::Medic)
    {
        return false;
    }
    if (const Unit *u = registry.Get<Unit>(target))
    {
        if (u->health <= 0.0f || u->teamID != medic.teamID || !IsHealableUnit(*u) ||
            IsEmbarked(registry, target))
        {
            return false;
        }
        if (u->health >= BaseStats(u->type).health)
        {
            return false;
        }
        outPos = u->position;
        return true;
    }
    return false;
}

bool CanHealTarget(const Registry &registry, const Unit &medic, Entity target)
{
    Vector2 aim = {};
    return HealAim(registry, medic, target, aim);
}

void CollectAreaRepairCandidates(Registry &registry, Rectangle worldArea, int teamID,
                                 std::vector<Entity> &out)
{
    std::vector<Entity> units;
    QueryUnitsInRect(registry, worldArea, teamID, units);
    for (const Entity id : units)
    {
        const Unit *unit = registry.Get<Unit>(id);
        if (unit == nullptr || unit->health >= BaseStats(unit->type).health)
        {
            continue;
        }
        if (IsRepairableUnit(*unit) || IsHealableUnit(*unit))
        {
            out.push_back(id);
        }
    }
    std::vector<Entity> buildings;
    QueryBuildingsInRect(registry, worldArea, teamID, buildings);
    for (const Entity id : buildings)
    {
        const Building *building = registry.Get<Building>(id);
        if (building != nullptr && building->state == BuildingState::Operational &&
            building->health < building->maxHealth)
        {
            out.push_back(id);
        }
    }
}

int AssignAreaRepair(const Registry &registry, const std::vector<Entity> &engineers,
                     const std::vector<Entity> &candidates,
                     std::vector<RepairAssignment> &out)
{
    std::vector<bool> claimed(candidates.size(), false);
    int assigned = 0;
    for (const Entity engineerId : engineers)
    {
        const Unit *engineer = registry.Get<Unit>(engineerId);
        if (engineer == nullptr)
        {
            continue;
        }
        float bestDistSq = -1.0f;
        std::size_t bestIndex = 0;
        bool found = false;
        for (std::size_t i = 0; i < candidates.size(); ++i)
        {
            if (claimed[i] || candidates[i] == engineerId)
            {
                continue;
            }
            const bool valid = engineer->type == UnitType::Medic
                                   ? CanHealTarget(registry, *engineer, candidates[i])
                                   : CanRepairTarget(registry, *engineer, candidates[i]);
            if (!valid)
            {
                continue;
            }
            const Vector2 goal = RepairCandidatePos(registry, candidates[i]);
            const float dx = goal.x - engineer->position.x;
            const float dy = goal.y - engineer->position.y;
            const float distSq = dx * dx + dy * dy;
            if (!found || distSq < bestDistSq)
            {
                bestDistSq = distSq;
                bestIndex = i;
                found = true;
            }
        }
        if (found)
        {
            claimed[bestIndex] = true;
            out.push_back({ engineerId, candidates[bestIndex] });
            ++assigned;
        }
    }
    return assigned;
}

cc::IVec2 RepairApproachTile(const TileMap &map, cc::IVec2 aimTile)
{
    if (!map.InBounds(aimTile))
    {
        return aimTile;
    }
    if (!map.IsBlocked(aimTile))
    {
        return aimTile;
    }
    for (int ring = 1; ring <= 6; ++ring)
    {
        for (int dy = -ring; dy <= ring; ++dy)
        {
            for (int dx = -ring; dx <= ring; ++dx)
            {
                if (dx * dx + dy * dy > ring * ring)
                {
                    continue;
                }
                const cc::IVec2 tile{ aimTile.x + dx, aimTile.y + dy };
                if (map.InBounds(tile) && !map.IsBlocked(tile))
                {
                    return tile;
                }
            }
        }
    }
    return aimTile;
}
