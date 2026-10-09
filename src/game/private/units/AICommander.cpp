#include "units/AICommander.h"

#include <algorithm>

#include "economy/Building.h"
#include "units/Extensions.h"
#include "units/Formation.h"
#include "economy/Nodes.h"
#include "world/Pathfinder.h"
#include "world/TileMap.h"
#include "units/UnitStats.h"

AIDifficultyParams ParamsFor(AIDifficulty difficulty)
{
    AIDifficultyParams params;
    switch (difficulty)
    {
    case AIDifficulty::Easy:
        params.harvesters = 1;
        params.waveThreshold = 3;
        params.scoutInterval = 60.0f;
        params.relaunchCooldown = 30.0f;
        params.retreats = false;
        params.composition = { UnitType::RifleInfantry };
        break;
    case AIDifficulty::Medium:
        params.harvesters = 2;
        params.waveThreshold = 5;
        params.scoutInterval = 30.0f;
        params.relaunchCooldown = 20.0f;
        params.retreats = false;
        params.composition = { UnitType::RifleInfantry, UnitType::RifleInfantry, UnitType::LightTank };
        break;
    case AIDifficulty::Hard:
        params.harvesters = 3;
        params.waveThreshold = 4;
        params.reserveUnits = 2;
        params.scoutInterval = 15.0f;
        params.relaunchCooldown = 15.0f;
        params.retreats = true;
        params.composition = { UnitType::RifleInfantry, UnitType::LightTank, UnitType::Artillery,
                               UnitType::IFV };
        break;
    }
    return params;
}

AICommander::AICommander(Registry &registry, TileMap &map, ResourceNodes &nodes,
                         EventDispatcher &events, int teamID, AIDifficulty difficulty,
                         cc::IVec2 homeTile, cc::IVec2 enemyTile)
    : registry_(registry), map_(map), nodes_(nodes), factory_(registry, resources_, events),
      params_(ParamsFor(difficulty)), teamID_(teamID), difficulty_(difficulty), homeTile_(homeTile),
      enemyTile_(enemyTile), rallyTile_(homeTile + cc::IVec2(0, 3)), lastSeenEnemy_(enemyTile)
{
    scoutTimer_ = params_.scoutInterval;
    timeSinceLaunch_ = params_.relaunchCooldown;
}

int AICommander::TeamID() const
{
    return teamID_;
}

AIDifficulty AICommander::Difficulty() const
{
    return difficulty_;
}

int AICommander::CombatUnitCount() const
{
    int count = 0;
    registry_.Each<Unit>([&](Entity, const Unit &unit) {
        if (unit.teamID == teamID_ && unit.health > 0.0f && unit.type != UnitType::Engineer)
        {
            ++count;
        }
    });
    return count;
}

int AICommander::HarvesterCount() const
{
    int count = 0;
    registry_.Each<Unit>([&](Entity, const Unit &unit) {
        if (unit.teamID == teamID_ && unit.health > 0.0f && unit.type == UnitType::Engineer)
        {
            ++count;
        }
    });
    return count;
}

int AICommander::WavesLaunched() const
{
    return wavesLaunched_;
}

bool AICommander::HasBootcamp() const
{
    bool found = false;
    registry_.Each<Building>([&](Entity, const Building &building) {
        if (building.teamID == teamID_ && building.type == BuildingType::Bootcamp &&
            building.state == BuildingState::Operational)
        {
            found = true;
        }
    });
    return found;
}

bool AICommander::HasWorkshop() const
{
    bool found = false;
    registry_.Each<Building>([&](Entity, const Building &building) {
        if (building.teamID == teamID_ && building.type == BuildingType::Workshop &&
            building.state == BuildingState::Operational)
        {
            found = true;
        }
    });
    return found;
}

bool AICommander::HasScouted() const
{
    return scouted_;
}

cc::IVec2 AICommander::LastSeenEnemy() const
{
    return lastSeenEnemy_;
}

void AICommander::Reset(AIDifficulty difficulty, cc::IVec2 homeTile, cc::IVec2 enemyTile,
                        int teamID)
{
    params_ = ParamsFor(difficulty);
    difficulty_ = difficulty;
    teamID_ = teamID;
    homeTile_ = homeTile;
    enemyTile_ = enemyTile;
    rallyTile_ = homeTile + cc::IVec2(0, 3);
    lastSeenEnemy_ = enemyTile;
    resources_ = ResourceSystem();
    queueBootcamp_.ResetForMatch();
    queueWorkshop_.ResetForMatch();
    harvesters_.clear();
    teams_.clear();
    compIndex_ = 0;
    bootcampCursor_ = 0;
    workshopCursor_ = 0;
    scouted_ = false;
    scoutTimer_ = params_.scoutInterval;
    timeSinceLaunch_ = params_.relaunchCooldown;
    wavesLaunched_ = 0;
}

void AICommander::SetupBase()
{
    resources_.AddIron(1000);
    resources_.AddOil(500);

    PlaceBuilding(registry_, map_, BuildingType::Base, teamID_, homeTile_.x, homeTile_.y, &nodes_);
    const cc::IVec2 depotSpots[] = { { 2, 0 }, { 0, 2 }, { -1, 0 }, { 0, -2 } };
    for (const cc::IVec2 &spot : depotSpots)
    {
        if (PlaceBuilding(registry_, map_, BuildingType::ResourceDepot, teamID_, homeTile_.x + spot.x,
                          homeTile_.y + spot.y, &nodes_) != kInvalidEntity)
        {
            break;
        }
    }
    const cc::IVec2 bootcampSpots[] = { { 0, 2 }, { 3, 0 },  { -2, 2 }, { 0, -3 },
                                        { -2, 0 }, { 0, -2 }, { 2, -2 }, { -3, 0 } };
    for (const cc::IVec2 &spot : bootcampSpots)
    {
        const Entity id = PlaceBuilding(registry_, map_, BuildingType::Bootcamp, teamID_,
                                        homeTile_.x + spot.x, homeTile_.y + spot.y, &nodes_);
        if (id != kInvalidEntity)
        {
            registry_.Get<Building>(id)->rallyTile = rallyTile_;
            break;
        }
    }
    const cc::IVec2 workshopSpots[] = { { 3, 2 }, { -3, 3 },  { 0, 5 },  { 0, -4 },
                                        { 4, -3 }, { -4, -2 }, { 5, 1 }, { -4, 1 } };
    bool workshopPlaced = false;
    for (const cc::IVec2 &spot : workshopSpots)
    {
        const Entity id = PlaceBuilding(registry_, map_, BuildingType::Workshop, teamID_,
                                        homeTile_.x + spot.x, homeTile_.y + spot.y, &nodes_);
        if (id != kInvalidEntity)
        {
            registry_.Get<Building>(id)->rallyTile = rallyTile_;
            workshopPlaced = true;
            break;
        }
    }
    for (int ring = 1; !workshopPlaced && ring <= 8; ++ring)
    {
        for (int dy = -ring; !workshopPlaced && dy <= ring; ++dy)
        {
            for (int dx = -ring; !workshopPlaced && dx <= ring; ++dx)
            {
                const Entity id =
                    PlaceBuilding(registry_, map_, BuildingType::Workshop, teamID_,
                                  homeTile_.x + dx, homeTile_.y + dy, &nodes_);
                if (id != kInvalidEntity)
                {
                    registry_.Get<Building>(id)->rallyTile = rallyTile_;
                    workshopPlaced = true;
                }
            }
        }
    }
    factory_.Spawn(UnitType::RifleInfantry, teamID_,
                   cc::ToRaylib(cc::TileToWorld(homeTile_.x, homeTile_.y)));
}

void AICommander::SetOccupancy(OccupancyGrid *occ)
{
    occ_ = occ;
}

void AICommander::OrderMove(Unit &unit, Entity id, Vector2 dest)
{
    if (occ_ != nullptr)
    {
        IssuePathOrderFootprint(unit, GetOrders(registry_, id), GetMover(registry_, id), map_,
                                *occ_, dest, id, registry_.Generation(id));
    }
    else
    {
        IssuePathOrder(unit, GetOrders(registry_, id), GetMover(registry_, id), map_, dest);
    }
}

void AICommander::OrderAttackMove(Unit &unit, Entity id, Vector2 dest)
{
    if (occ_ != nullptr)
    {
        IssueAttackMoveOrderFootprint(unit, GetOrders(registry_, id), GetMover(registry_, id),
                                      map_, *occ_, dest, id, registry_.Generation(id));
    }
    else
    {
        IssueAttackMoveOrder(unit, GetOrders(registry_, id), GetMover(registry_, id), map_, dest);
    }
}

void AICommander::Update(float dt)
{
    if (dt <= 0.0f)
    {
        return;
    }
    UpdateBaseIncome(registry_, resources_, dt, teamID_);
    nodes_.GatherTick(registry_, resources_, dt, teamID_);
    auto pumpQueue = [&](ProductionQueue &queue, BuildingType type, int &cursor) {
        const Entity producerId = ProducerBuildingAt(registry_, type, teamID_, cursor);
        if (producerId == kInvalidEntity)
        {
            return;
        }
        const Building *producer = registry_.Get<Building>(producerId);
        if (producer == nullptr)
        {
            return;
        }
        const cc::IVec2 rally = producer->rallyTile;
        const cc::IVec2 spawnTile = BuildingSpawnTile(map_, occ_, *producer);
        const Vector2 spawnPos = cc::ToRaylib(cc::TileToWorld(spawnTile.x, spawnTile.y));
        const Entity spawned = queue.Update(factory_, resources_, teamID_, spawnPos, dt, &map_, occ_);
        if (spawned == kInvalidEntity)
        {
            return;
        }
        ++cursor;
        if (Unit *fresh = registry_.Get<Unit>(spawned))
        {
            if (!(cc::WorldToTile(cc::ToGlm(fresh->position)) == rally))
            {
                OrderMove(*fresh, spawned,
                          cc::ToRaylib(cc::TileToWorld(rally.x, rally.y)));
            }
        }
    };
    if (HasBootcamp())
    {
        pumpQueue(queueBootcamp_, BuildingType::Bootcamp, bootcampCursor_);
    }
    if (HasWorkshop())
    {
        pumpQueue(queueWorkshop_, BuildingType::Workshop, workshopCursor_);
    }
    MaintainHarvesters();
    MaintainProduction();
    timeSinceLaunch_ += dt;
    MaybeLaunchWave();
    UpdateTeams(dt);
    ScoutTick(dt);
    if (params_.retreats)
    {
        RetreatTick();
    }
}

bool AICommander::FindLiveIron(cc::IVec2 &outTile) const
{
    bool found = false;
    int best = 0;
    nodes_.Each([&](const ResourceNode &node) {
        if (node.kind != ResourceKind::Iron || node.IsDepleted())
        {
            return;
        }
        const int dist = (node.tile.x - homeTile_.x) * (node.tile.x - homeTile_.x) +
                         (node.tile.y - homeTile_.y) * (node.tile.y - homeTile_.y);
        if (!found || dist < best)
        {
            found = true;
            best = dist;
            outTile = node.tile;
        }
    });
    return found;
}

void AICommander::OrderHarvesterToIron(Entity harvester)
{
    Unit *unit = registry_.Get<Unit>(harvester);
    if (unit == nullptr)
    {
        return;
    }
    cc::IVec2 iron{ 0, 0 };
    const cc::IVec2 dest = FindLiveIron(iron) ? iron : homeTile_;
    OrderMove(*unit, harvester, cc::ToRaylib(cc::TileToWorld(dest.x, dest.y)));
}

void AICommander::MaintainHarvesters()
{
    std::vector<Entity> alive;
    alive.reserve(harvesters_.size());
    for (Entity id : harvesters_)
    {
        const Unit *unit = registry_.Get<Unit>(id);
        if (unit != nullptr && unit->health > 0.0f)
        {
            alive.push_back(id);
        }
    }
    harvesters_.swap(alive);
    while (static_cast<int>(harvesters_.size()) < params_.harvesters)
    {
        const Entity id = factory_.Spawn(UnitType::Engineer, teamID_,
                                         cc::ToRaylib(cc::TileToWorld(homeTile_.x, homeTile_.y)));
        if (id == kInvalidEntity)
        {
            break;
        }
        OrderHarvesterToIron(id);
        harvesters_.push_back(id);
    }
}

void AICommander::MaintainProduction()
{
    if (params_.composition.empty())
    {
        return;
    }
    const int desired = params_.waveThreshold + 2 + params_.reserveUnits;
    int pipeline = CombatUnitCount() + static_cast<int>(queueBootcamp_.Size()) +
                   static_cast<int>(queueWorkshop_.Size());
    int guard = 0;
    while (pipeline < desired && guard < desired)
    {
        ++guard;
        const UnitType type = params_.composition[compIndex_ % params_.composition.size()];
        ++compIndex_;
        const bool infantry = ProductionCategoryOf(type) == ProductionCategory::Infantry;
        ProductionQueue &queue = infantry ? queueBootcamp_ : queueWorkshop_;
        if (infantry ? !HasBootcamp() : !HasWorkshop())
        {
            continue;
        }
        if (!queue.Enqueue(resources_, type))
        {
            break;
        }
        ++pipeline;
    }
}

void AICommander::PruneTeam(AITeam &team)
{
    std::vector<Entity> alive;
    alive.reserve(team.members.size());
    for (Entity id : team.members)
    {
        const Unit *unit = registry_.Get<Unit>(id);
        if (unit != nullptr && unit->health > 0.0f && unit->teamID == teamID_)
        {
            alive.push_back(id);
        }
    }
    team.members.swap(alive);
}

bool AICommander::TeamAllIdle(const AITeam &team)
{
    for (Entity id : team.members)
    {
        const Mover *mover = FindMover(registry_, id);
        if (mover != nullptr && (mover->hasMoveOrder || mover->hasPath))
        {
            return false;
        }
    }
    return true;
}

void AICommander::IssueTeamStep(AITeam &team)
{
    if (team.step >= team.script.size())
    {
        return;
    }
    const AIMissionStep &mission = team.script[team.step];
    const std::vector<cc::IVec2> offsets = formation::FormationOffsets(team.members.size());
    for (std::size_t i = 0; i < team.members.size(); ++i)
    {
        Unit *unit = registry_.Get<Unit>(team.members[i]);
        if (unit == nullptr)
        {
            continue;
        }
        const cc::IVec2 slot = mission.target + offsets[i];
        const Vector2 dest = cc::ToRaylib(cc::TileToWorld(slot.x, slot.y));
        switch (mission.kind)
        {
        case AIMissionKind::Move:
            OrderMove(*unit, team.members[i], dest);
            break;
        case AIMissionKind::Guard:
            OrderMove(*unit, team.members[i], dest);
            if (CombatState *combat = registry_.Get<CombatState>(team.members[i]))
            {
                SetStance(*unit, GetOrders(registry_, team.members[i]), *combat, Stance::Guard);
            }
            break;
        case AIMissionKind::AttackMove:
        default:
            OrderAttackMove(*unit, team.members[i], dest);
            break;
        }
    }
}

void AICommander::MusterTeams()
{
    for (AITeam &team : teams_)
    {
        PruneTeam(team);
    }
    std::vector<Entity> unassigned;
    registry_.Each<Unit>([&](Entity id, const Unit &unit) {
        if (unit.teamID != teamID_ || unit.health <= 0.0f || unit.type == UnitType::Engineer)
        {
            return;
        }
        for (const AITeam &team : teams_)
        {
            if (std::find(team.members.begin(), team.members.end(), id) != team.members.end())
            {
                return;
            }
        }
        unassigned.push_back(id);
    });
    if (unassigned.empty())
    {
        return;
    }
    bool hasGuard = false;
    for (const AITeam &team : teams_)
    {
        if (team.role == AITeamRole::Guard && !team.members.empty())
        {
            hasGuard = true;
        }
    }
    if (!hasGuard &&
        static_cast<int>(unassigned.size()) >= params_.waveThreshold + 4)
    {
        AITeam guard;
        guard.role = AITeamRole::Guard;
        guard.members.assign(unassigned.end() - 2, unassigned.end());
        unassigned.erase(unassigned.end() - 2, unassigned.end());
        guard.script = { { AIMissionKind::Guard, rallyTile_, 0.0f } };
        teams_.push_back(guard);
    }
    if (unassigned.size() >= 8)
    {
        std::sort(unassigned.begin(), unassigned.end(), [&](Entity a, Entity b) {
            const Unit *ua = registry_.Get<Unit>(a);
            const Unit *ub = registry_.Get<Unit>(b);
            const float sa = ua != nullptr ? BaseStats(ua->type).speed : 0.0f;
            const float sb = ub != nullptr ? BaseStats(ub->type).speed : 0.0f;
            return sa > sb;
        });
        const std::size_t harassCount = std::max<std::size_t>(2, unassigned.size() / 3);
        AITeam harass;
        harass.role = AITeamRole::Harass;
        harass.members.assign(unassigned.begin(), unassigned.begin() + harassCount);
        const int dx = lastSeenEnemy_.x - homeTile_.x;
        const int dy = lastSeenEnemy_.y - homeTile_.y;
        cc::IVec2 flank{ lastSeenEnemy_.x - dy / 4, lastSeenEnemy_.y + dx / 4 };
        flank.x = std::clamp(flank.x, 0, map_.Width() - 1);
        flank.y = std::clamp(flank.y, 0, map_.Height() - 1);
        harass.script = { { AIMissionKind::Move, flank, 30.0f },
                          { AIMissionKind::AttackMove, lastSeenEnemy_, 120.0f } };
        teams_.push_back(harass);
        unassigned.erase(unassigned.begin(), unassigned.begin() + harassCount);
    }
    if (!unassigned.empty())
    {
        bool hasIdleMain = false;
        for (const AITeam &team : teams_)
        {
            if (team.role == AITeamRole::Main && !team.active && !team.members.empty())
            {
                hasIdleMain = true;
            }
        }
        if (!hasIdleMain)
        {
            AITeam main;
            main.role = AITeamRole::Main;
            main.members = unassigned;
            main.script = { { AIMissionKind::AttackMove, lastSeenEnemy_, 120.0f } };
            teams_.push_back(main);
        }
        else
        {
            for (AITeam &team : teams_)
            {
                if (team.role == AITeamRole::Main && !team.active && !team.members.empty())
                {
                    team.members.insert(team.members.end(), unassigned.begin(), unassigned.end());
                    break;
                }
            }
        }
    }
}

void AICommander::LaunchTeams()
{
    for (AITeam &team : teams_)
    {
        if (!team.active && !team.members.empty())
        {
            team.step = 0;
            team.stepTime = 0.0f;
            team.active = true;
            IssueTeamStep(team);
        }
    }
    ++wavesLaunched_;
    timeSinceLaunch_ = 0.0f;
}

void AICommander::UpdateTeams(float dt)
{
    for (AITeam &team : teams_)
    {
        if (!team.active)
        {
            continue;
        }
        PruneTeam(team);
        if (team.members.empty() || team.step >= team.script.size())
        {
            team.active = false;
            team.members.clear();
            continue;
        }
        team.stepTime += dt;
        const AIMissionStep &mission = team.script[team.step];
        if (mission.timeout > 0.0f && team.stepTime >= mission.timeout)
        {
            ++team.step;
            team.stepTime = 0.0f;
            if (team.step < team.script.size())
            {
                IssueTeamStep(team);
            }
            else
            {
                team.active = false;
                team.members.clear();
            }
            continue;
        }
        if (team.step + 1 == team.script.size() &&
            mission.kind == AIMissionKind::AttackMove && TeamAllIdle(team))
        {
            team.active = false;
            team.members.clear();
        }
    }
}

void AICommander::MaybeLaunchWave()
{
    MusterTeams();
    const AITeam *main = nullptr;
    for (const AITeam &team : teams_)
    {
        if (team.role == AITeamRole::Main && !team.active && !team.members.empty() &&
            static_cast<int>(team.members.size()) >= params_.waveThreshold)
        {
            main = &team;
            break;
        }
    }
    if (main == nullptr || timeSinceLaunch_ < params_.relaunchCooldown)
    {
        return;
    }
    LaunchTeams();
}

void AICommander::ScoutTick(float dt)
{
    scoutTimer_ -= dt;
    if (scoutTimer_ > 0.0f)
    {
        return;
    }
    const Entity scout = factory_.Spawn(UnitType::RifleInfantry, teamID_,
                                        cc::ToRaylib(cc::TileToWorld(homeTile_.x, homeTile_.y)));
    scoutTimer_ = params_.scoutInterval;
    if (scout == kInvalidEntity)
    {
        return;
    }
    if (Unit *unit = registry_.Get<Unit>(scout))
    {
        OrderMove(*unit, scout, cc::ToRaylib(cc::TileToWorld(enemyTile_.x, enemyTile_.y)));
    }
    scouted_ = true;
    lastSeenEnemy_ = enemyTile_;
}

void AICommander::RetreatTick()
{
    const Vector2 home = cc::ToRaylib(cc::TileToWorld(homeTile_.x, homeTile_.y));
    RetreatIfLowHP(registry_, map_, occ_, home, teamID_, kRetreatHealthFraction, false);
}
