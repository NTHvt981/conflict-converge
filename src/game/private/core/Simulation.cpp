// Simulation.cpp - the per-frame match tick, extracted from Game::Update
// (H6 slice 1) into its own class so the tick is headless-drivable.

#include "Simulation.h"

#include "Building.h" // auto-repair, construction, factory gate
#include "Shake.h"    // hit/death trauma
#include "Unit.h"     // movement frame, retreat, death sweep types
#include <cmath>
#include <vector>

Simulation::Simulation(Registry &registry, TileMap &map, OccupancyGrid &occ, FogOfWar &fog,
                       ResourceNodes &nodes, ProductionQueue &queue, UnitFactory &factory,
                       ResourceSystem &resources, AICommander &ai, AICommander &allyAI,
                       AICommander &enemyAI2, Art &art, Audio &audio, Pings &pings,
                       MenuFlow &menu, Minimap &minimap, const WorldState &worldState,
                       DamageNumbers &damageNumbers, EventDispatcher &events,
                       const Vector2 &rallyPos, const int &autoAddGroupBit,
                       const bool &sandboxMode, const bool &worldIs2v2,
                       const bool &playerAutoRepair, const float &autoRepairCap,
                       float &shakeTrauma, MenuState &lastOutcomeState)
    : registry_(registry)
    , map_(map)
    , occ_(occ)
    , fog_(fog)
    , nodes_(nodes)
    , queue_(queue)
    , factory_(factory)
    , resources_(resources)
    , ai_(ai)
    , allyAI_(allyAI)
    , enemyAI2_(enemyAI2)
    , art_(art)
    , audio_(audio)
    , pings_(pings)
    , menu_(menu)
    , minimap_(minimap)
    , worldState_(worldState)
    , damageNumbers_(damageNumbers)
    , events_(events)
    , rallyPos_(rallyPos)
    , autoAddGroupBit_(autoAddGroupBit)
    , sandboxMode_(sandboxMode)
    , worldIs2v2_(worldIs2v2)
    , playerAutoRepair_(playerAutoRepair)
    , autoRepairCap_(autoRepairCap)
    , shakeTrauma_(shakeTrauma)
    , lastOutcomeState_(lastOutcomeState)
{
}

void Simulation::ResetForMatch()
{
    lastBuildingCount_ = 0;
    lastDepletedCount_ = 0;
    lastQueueSize_ = 0;
    attackSfxTimer_ = 0.0f;
    hasFactory_ = false;
    // Fresh replay recording for this match (last match only).
    replayRecording_ = true;
    replayTimer_ = 0.0f;
    replayIndex_ = 0;
    replayCount_ = 0;
}

void Simulation::ResetEdgePolls()
{
    lastBuildingCount_ = 0;
    lastDepletedCount_ = 0;
    lastQueueSize_ = 0;
    attackSfxTimer_ = 0.0f;
    hasFactory_ = false;
}

void Simulation::StopRecording()
{
    replayRecording_ = false;
}

void Simulation::SetReplayCount(int count)
{
    replayCount_ = count;
}

int Simulation::ReplayCount() const
{
    return replayCount_;
}

void Simulation::RefreshFactory()
{
    // Production dies with the structure — the queue gate and the HUD
    // panel share this recompute so the panel stays correct while paused.
    hasFactory_ = false;
    registry_.Each<Building>([&](Entity, const Building &building) {
        if (building.teamID == 0 && building.type == BuildingType::Factory &&
            building.state == BuildingState::Operational)
        {
            hasFactory_ = true;
        }
    });
}

bool Simulation::HasFactory() const
{
    return hasFactory_;
}

void Simulation::Announce(EventType type)
{
    // Bare-event announcer for the game-state + UI event types.
    Event bare;
    bare.type = type;
    events_.Dispatch(bare);
}

void Simulation::Step(float dt)
{
    // Rebuild visibility from current positions before anyone acquires.
    fog_.Recompute(registry_);
    // Occupancy pre-pass and stall fix: per-unit driver with fog gate,
    // overlap separation, and separation-stall detection, all as one
    // pipeline (see RunUnitMovementFrame) so the game loop and tests
    // can't drift apart on this sequencing.
    RunUnitMovementFrame(registry_, map_, occ_, &fog_, dt);
    // Collect the fallen, then destroy through the factory so
    // UnitDestroyed is announced (destroying inside Each would invalidate it).
    std::vector<Entity> dead;
    registry_.Each<Unit>([&](Entity id, const Unit &unit) {
        if (unit.health <= 0.0f)
        {
            dead.push_back(id);
        }
    });
    for (Entity id : dead)
    {
        if (const Unit *corpse = registry_.Get<Unit>(id))
        {
            // Death burst at the corpse before teardown.
            art_.ParticlesPool().SpawnBurst(
                { corpse->position.x + 32.0f, corpse->position.y + 32.0f }, ORANGE, 24,
                120.0f, 0.6f);
            // Release occupancy tiles before teardown (owned:
            // a corpse shoved onto a live unit's tile must not wipe it).
            const cc::IVec2 anchor = cc::WorldToTile(cc::ToGlm(corpse->position));
            occ_.ReleaseFootprintOwned(anchor, corpse->footprintWidth,
                                       corpse->footprintHeight, id,
                                       registry_.Generation(id));
        }
        factory_.DestroyUnit(id);
    }
    // Edge-triggered battle sounds (one explosion per wipe, not per corpse).
    if (!dead.empty())
    {
        audio_.Play(SfxId::Explosion);
        shakeTrauma_ = AddShakeTrauma(shakeTrauma_, kShakeDeathTrauma);
    }
    // Impact sparks on fresh hits + muzzle sparks while telegraphing.
    registry_.Each<Unit>([&](Entity, const Unit &unit) {
        const Vector2 center = { unit.position.x + 32.0f, unit.position.y + 32.0f };
        if (unit.hitFlashTime > 0.20f)
        {
            art_.ParticlesPool().SpawnBurst(center, YELLOW, 6, 90.0f, 0.25f);
            // Floating number, once per hit: same edge trigger, spawned
            // over the body top (the old fixed text's anchor). Kept out
            // of Draw on purpose — pool mutation belongs in update.
            damageNumbers_.Spawn({ center.x, unit.position.y - 2.0f },
                                 unit.lastDamageTaken);
            shakeTrauma_ = AddShakeTrauma(shakeTrauma_, kShakeHitTrauma);
            // QoL pings: same "just got hit" detector, no Combat.h
            // changes (per-kind floor inside Pings stops spam).
            // Team 0 is the player.
            if (unit.teamID == 0)
            {
                pings_.Raise(center, PingKind::UnderAttack);
            }
        }
        if (unit.phase == AttackPhase::WindUp)
        {
            if (const Unit *target = registry_.Get<Unit>(unit.target))
            {
                const Vector2 dir = { target->position.x - unit.position.x,
                                      target->position.y - unit.position.y };
                const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
                if (len > 1.0f)
                {
                    art_.ParticlesPool().SpawnBurst(
                        { center.x + dir.x / len * 20.0f, center.y + dir.y / len * 20.0f },
                        ORANGE, 2, 40.0f, 0.15f);
                }
            }
        }
    });
    art_.ParticlesPool().Update(dt);
    damageNumbers_.Update(dt);
    attackSfxTimer_ -= dt;
    bool windingUp = false;
    registry_.Each<Unit>([&](Entity, const Unit &unit) {
        if (unit.phase == AttackPhase::WindUp)
        {
            windingUp = true;
        }
    });
    if (windingUp && attackSfxTimer_ <= 0.0f)
    {
        audio_.Play(SfxId::Attack);
        attackSfxTimer_ = 0.12f;
    }
    int buildingCount = 0;
    registry_.Each<Building>([&](Entity, const Building &building) {
        if (building.state == BuildingState::Operational)
        {
            ++buildingCount;
        }
    });
    if (buildingCount > lastBuildingCount_)
    {
        audio_.Play(SfxId::Place);
    }
    lastBuildingCount_ = buildingCount;
    int depletedCount = 0;
    nodes_.Each([&](const ResourceNode &node) {
        if (node.IsDepleted())
        {
            ++depletedCount;
        }
    });
    if (depletedCount > lastDepletedCount_)
    {
        audio_.Play(SfxId::Deplete);
        Announce(EventType::ResourceDepleted); // Resource event
    }
    lastDepletedCount_ = depletedCount;
    // Economy tick: base trickle, node respawn, harvest, production.
    UpdateBaseIncome(registry_, resources_, dt, 0);
    nodes_.Update(dt);
    nodes_.GatherTick(registry_, resources_, dt, 0); // team 0 crew only (AI gathers its own)
    RefreshFactory();
    if (hasFactory_)
    {
        const Entity spawned = queue_.Update(factory_, resources_, 0, rallyPos_, dt);
        if (spawned != kInvalidEntity && autoAddGroupBit_ >= 0)
        {
            if (Unit *fresh = registry_.Get<Unit>(spawned))
            {
                fresh->controlGroups |= (1u << static_cast<unsigned int>(autoAddGroupBit_));
            }
        }
    }
    // Production-ordered event on every queue growth (panel
    // buttons and the seed enqueues both flow through here).
    if (queue_.Size() > static_cast<std::size_t>(lastQueueSize_))
    {
        Announce(EventType::ProductionOrdered);
    }
    lastQueueSize_ = static_cast<int>(queue_.Size());
    // QoL snapshot replay: capture a full-state frame every 2s while
    // the match runs (best-effort: a failed write retries next tick
    // without consuming the frame number; stops at the frame cap).
    if (replayRecording_)
    {
        replayTimer_ += dt;
        if (replayTimer_ >= 2.0f)
        {
            replayTimer_ = 0.0f;
            if (replayIndex_ < kReplayMaxFrames &&
                SaveWorld(worldState_, ReplayFramePath(kReplayDir, replayIndex_)))
            {
                ++replayIndex_;
                replayCount_ = replayIndex_;
            }
        }
    }
    // QoL building auto-repair (player team 0 only; AI economy is
    // tuned and Engineers already repair free — see Building.h).
    if (playerAutoRepair_)
    {
        UpdateBuildingAutoRepair(registry_, resources_, dt, 0, autoRepairCap_);
    }
    UpdateBuildingConstruction(registry_, dt); // sites -> Operational, all teams
    // QoL player auto-retreat: opted-in units below threshold fall back
    // to the rally point (or the nearest owned Base when no rally was
    // ever placed). Shared RetreatIfLowHP with the AI's RetreatTick.
    {
        const Vector2 home = ResolvePlayerRetreatHome(registry_, rallyPos_);
        RetreatIfLowHP(registry_, map_, &occ_, home, 0, kRetreatHealthFraction, true);
    }
    if (!sandboxMode_)
    {
        ai_.Update(dt); // Enemy build order, waves, scouting, retreat
    }
    // 2v2 overflow commanders tick only in 2v2 matches (see worldIs2v2:
    // count-gating wakes parked commanders in 1v1 via shared teams).
    if (worldIs2v2_)
    {
        allyAI_.Update(dt);   // allied build order + waves beside the player
        enemyAI2_.Update(dt); // second enemy front
    }
    // Periodic minimap refresh (terrain blocks + unit dots).
    if (minimap_.PollRefresh(dt))
    {
        BeginTextureMode(minimap_.target);
        ClearBackground(Color{ 20, 60, 20, 255 }); // dark grass base
        for (int y = 0; y < map_.Height(); ++y)
        {
            for (int x = 0; x < map_.Width(); ++x)
            {
                const TerrainType terrain = map_.Get({ x, y });
                if (terrain == TerrainType::Grass)
                {
                    continue;
                }
                const Vector2 corner = minimap_.WorldToMinimap(cc::ToRaylib(cc::TileToWorld(x, y)),
                                                              map_.Width(), map_.Height());
                const Color tint = terrain == TerrainType::Water    ? DARKBLUE
                                 : terrain == TerrainType::Forest ? Color{ 20, 90, 20, 255 }
                                 : terrain == TerrainType::Rock   ? GRAY
                                                                  : DARKGRAY;
                DrawRectangleV({ corner.x - minimap_.screenRect.x, corner.y - minimap_.screenRect.y },
                               { 8.0f, 8.0f }, tint);
            }
        }
        registry_.Each<Unit>([&](Entity, const Unit &unit) {
            // Enemies only appear when a team-0 unit sees their tile.
            if (unit.teamID != 0 &&
                !fog_.IsVisible(0, cc::WorldToTile(cc::ToGlm(unit.position))))
            {
                return;
            }
            const Vector2 center = { unit.position.x + 32.0f, unit.position.y + 32.0f };
            const Vector2 dot =
                minimap_.WorldToMinimap(center, map_.Width(), map_.Height());
            DrawRectangle(static_cast<int>(dot.x - minimap_.screenRect.x) - 1,
                          static_cast<int>(dot.y - minimap_.screenRect.y) - 1, 3, 3,
                          art_.TeamTint(unit.teamID));
        });
        EndTextureMode();
    }
    // Decide terminal states from the living rosters.
    // Skipped in sandbox mode (no enemy side: instant Victory otherwise).
    if (!sandboxMode_)
    {
        menu_.ShowOutcome(TeamHasUnits(registry_, 0), TeamHasUnits(registry_, 1));
    }
    // Fanfare on the transition frame only.
    // Game-state events ride the same transition.
    if (menu_.state != lastOutcomeState_)
    {
        if (menu_.state == MenuState::Victory)
        {
            audio_.Play(SfxId::Victory);
            Announce(EventType::Victory);
        }
        else if (menu_.state == MenuState::GameOver)
        {
            audio_.Play(SfxId::Defeat);
            Announce(EventType::GameOver);
        }
        lastOutcomeState_ = menu_.state;
    }
}
