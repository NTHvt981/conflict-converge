#include "AICommander.h"

#include "Building.h"
#include "Formation.h"
#include "Nodes.h"
#include "Pathfinder.h"
#include "TileMap.h"
#include "UnitStats.h"

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
        params.composition = { UnitType::Infantry };
        break;
    case AIDifficulty::Medium:
        params.harvesters = 2;
        params.waveThreshold = 5;
        params.scoutInterval = 30.0f;
        params.relaunchCooldown = 20.0f;
        params.retreats = false;
        params.composition = { UnitType::Infantry, UnitType::Infantry, UnitType::LightTank };
        break;
    case AIDifficulty::Hard:
        params.harvesters = 3;
        params.waveThreshold = 4;
        params.reserveUnits = 2; // mass up: pipeline 8 vs Medium's 7, so the
                                 // lower threshold attacks sooner AND heavier
        params.scoutInterval = 15.0f;
        params.relaunchCooldown = 15.0f;
        params.retreats = true;
        params.composition = { UnitType::Infantry, UnitType::LightTank, UnitType::Artillery,
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
    timeSinceLaunch_ = params_.relaunchCooldown; // first wave may launch on threshold
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

bool AICommander::HasFactory() const
{
    bool found = false;
    registry_.Each<Building>([&](Entity, const Building &building) {
        if (building.teamID == teamID_ && building.type == BuildingType::Factory &&
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
    queue_ = ProductionQueue();
    harvesters_.clear();
    compIndex_ = 0;
    scouted_ = false;
    scoutTimer_ = params_.scoutInterval;
    timeSinceLaunch_ = params_.relaunchCooldown;
    wavesLaunched_ = 0;
}

void AICommander::SetupBase()
{
    // Same starting funds as the demo player: fair rules.
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
    const cc::IVec2 factorySpots[] = { { 0, 2 }, { 3, 0 },  { -2, 2 }, { 0, -3 },
                                       { -2, 0 }, { 0, -2 }, { 2, -2 }, { -3, 0 } };
    for (const cc::IVec2 &spot : factorySpots)
    {
        if (PlaceBuilding(registry_, map_, BuildingType::Factory, teamID_, homeTile_.x + spot.x,
                          homeTile_.y + spot.y, &nodes_) != kInvalidEntity)
        {
            break;
        }
    }
    // Starting guard: the team must field a living unit from frame one, or
    // the menu outcome declares the match over before it begins.
    factory_.Spawn(UnitType::Infantry, teamID_,
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
        IssuePathOrderFootprint(unit, map_, *occ_, dest, id, registry_.Generation(id));
    }
    else
    {
        IssuePathOrder(unit, map_, dest);
    }
}

void AICommander::Update(float dt)
{
    if (dt <= 0.0f)
    {
        return;
    }
    UpdateBaseIncome(registry_, resources_, dt, teamID_);
    nodes_.GatherTick(registry_, resources_, dt, teamID_); // own crew, own ledger
    if (HasFactory())
    {
        // Production dies with the structure — razed AI stays down.
        queue_.Update(factory_, resources_, teamID_,
                      cc::ToRaylib(cc::TileToWorld(rallyTile_.x, rallyTile_.y)), dt);
    }
    MaintainHarvesters();
    MaintainProduction();
    timeSinceLaunch_ += dt;
    MaybeLaunchWave();
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
    // Prune the dead, then top up to the difficulty's harvester count.
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
            break; // broke: retry next tick when income lands
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
    // Keep the pipeline (fielded combat units + queued builds) at threshold.
    // Queue charges upfront, so this naturally paces itself on income.
    // Hard masses above the line via reserveUnits (its threshold stays low
    // per spec: sooner waves, but heavier ones).
    const int desired = params_.waveThreshold + 2 + params_.reserveUnits;
    int pipeline = CombatUnitCount() + static_cast<int>(queue_.Size());
    int guard = 0;
    while (pipeline < desired && guard < desired)
    {
        ++guard;
        if (!queue_.Enqueue(resources_, params_.composition[compIndex_ % params_.composition.size()]))
        {
            break; // insufficient funds: income will retry next tick
        }
        ++compIndex_;
        ++pipeline;
    }
}

void AICommander::MaybeLaunchWave()
{
    std::vector<Entity> army;
    registry_.Each<Unit>([&](Entity id, const Unit &unit) {
        if (unit.teamID == teamID_ && unit.health > 0.0f && unit.type != UnitType::Engineer)
        {
            army.push_back(id);
        }
    });
    if (static_cast<int>(army.size()) < params_.waveThreshold ||
        timeSinceLaunch_ < params_.relaunchCooldown)
    {
        return;
    }
    // Waves attack-move (not plain-move): marchers engage defenders on
    // contact instead of walking past them, then resume the advance. Plain
    // formation orders produced walk-through stalemates.
    // The march itself routes footprint-aware when occupancy is bound.
    const std::vector<cc::IVec2> offsets = formation::FormationOffsets(army.size());
    for (std::size_t i = 0; i < army.size(); ++i)
    {
        Unit *unit = registry_.Get<Unit>(army[i]);
        if (unit == nullptr)
        {
            continue;
        }
        const cc::IVec2 slot = lastSeenEnemy_ + offsets[i];
        const Vector2 dest = cc::ToRaylib(cc::TileToWorld(slot.x, slot.y));
        if (occ_ != nullptr)
        {
            IssueAttackMoveOrderFootprint(*unit, map_, *occ_, dest, army[i],
                                          registry_.Generation(army[i]));
        }
        else
        {
            IssueAttackMoveOrder(*unit, map_, dest);
        }
    }
    ++wavesLaunched_;
    timeSinceLaunch_ = 0.0f;
}

void AICommander::ScoutTick(float dt)
{
    scoutTimer_ -= dt;
    if (scoutTimer_ > 0.0f)
    {
        return;
    }
    const Entity scout = factory_.Spawn(UnitType::Infantry, teamID_,
                                        cc::ToRaylib(cc::TileToWorld(homeTile_.x, homeTile_.y)));
    // NOTE: the full-interval backoff below runs even when the spawn fails.
    // A fast retry here was tried and reverted: refilling scouts ahead of
    // the army re-tunes Hard's economy enough to flip the Medium-vs-Hard
    // outcome. Tuned as a set; do not adjust alone.
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
    lastSeenEnemy_ = enemyTile_; // waves rally on latest intel
}

void AICommander::RetreatTick()
{
    // Fighting withdrawal (attack-move, never plain move) toward the home
    // tile; shared implementation with the player-facing auto-retreat.
    const Vector2 home = cc::ToRaylib(cc::TileToWorld(homeTile_.x, homeTile_.y));
    RetreatIfLowHP(registry_, map_, occ_, home, teamID_, kRetreatHealthFraction, false);
}
