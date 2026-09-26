
#include "core/Simulation.h"

#include "economy/Building.h"
#include "units/Extensions.h"
#include "app/ui/Shake.h"
#include "units/Unit.h"
#include "world/Pathfinder.h"
#include <cmath>
#include <vector>

Simulation::Simulation(Subsystems &world, Subsystems &engine, MenuFlow &menu,
                       const WorldState &worldState, DamageNumbers &damageNumbers,
                       const int &autoAddGroupBit,
                       const bool &sandboxMode, const bool &worldIs2v2,
                       float &shakeTrauma, MenuState &lastOutcomeState)
    : registry_(world.Get<Registry>())
    , map_(world.Get<TileMap>())
    , occ_(world.Get<OccupancyGrid>())
    , fog_(world.Get<FogOfWar>())
    , nodes_(world.Get<ResourceNodes>())
    , bootcampQueue_(world.GetKeyed<ProductionQueue>("bootcamp"))
    , workshopQueue_(world.GetKeyed<ProductionQueue>("workshop"))
    , factory_(world.Get<UnitFactory>())
    , resources_(world.Get<ResourceSystem>())
    , ai_(world.GetKeyed<AICommander>("ai"))
    , allyAI_(world.GetKeyed<AICommander>("ally"))
    , enemyAI2_(world.GetKeyed<AICommander>("enemy2"))
    , art_(engine.Get<Art>())
    , audio_(engine.Get<Audio>())
    , pings_(world.Get<Pings>())
    , menu_(menu)
    , minimap_(world.Get<Minimap>())
    , worldState_(worldState)
    , damageNumbers_(damageNumbers)
    , events_(engine.Get<EventDispatcher>())
    , autoAddGroupBit_(autoAddGroupBit)
    , sandboxMode_(sandboxMode)
    , worldIs2v2_(worldIs2v2)
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
    hasBootcamp_ = false;
    hasWorkshop_ = false;
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
    hasBootcamp_ = false;
    hasWorkshop_ = false;
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

void Simulation::RefreshProducers()
{
    hasBootcamp_ = false;
    hasWorkshop_ = false;
    registry_.Each<Building>([&](Entity, const Building &building) {
        if (building.teamID != 0 || building.state != BuildingState::Operational)
        {
            return;
        }
        if (building.type == BuildingType::Bootcamp)
        {
            hasBootcamp_ = true;
        }
        else if (building.type == BuildingType::Workshop)
        {
            hasWorkshop_ = true;
        }
    });
}

bool Simulation::HasBootcamp() const
{
    return hasBootcamp_;
}

bool Simulation::HasWorkshop() const
{
    return hasWorkshop_;
}

void Simulation::Announce(EventType type)
{
    Event bare;
    bare.type = type;
    events_.Dispatch(bare);
}

void Simulation::Step(float dt)
{
    fog_.Recompute(registry_);
    RunUnitMovementFrame(registry_, map_, occ_, &fog_, dt);
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
            art_.ParticlesPool().SpawnBurst(
                { corpse->position.x + 32.0f, corpse->position.y + 32.0f }, ORANGE, 24,
                120.0f, 0.6f);
            const cc::IVec2 anchor = cc::WorldToTile(cc::ToGlm(corpse->position));
            occ_.ReleaseFootprintOwned(anchor, corpse->footprintWidth,
                                       corpse->footprintHeight, id,
                                       registry_.Generation(id));
        }
        factory_.DestroyUnit(id);
    }
    if (!dead.empty())
    {
        audio_.Play(SfxId::Explosion);
        shakeTrauma_ = AddShakeTrauma(shakeTrauma_, kShakeDeathTrauma);
    }
    registry_.Each<Unit>([&](Entity id, const Unit &unit) {
        const Vector2 center = { unit.position.x + 32.0f, unit.position.y + 32.0f };
        const CombatState *combat = FindCombatState(registry_, id);
        if (combat != nullptr && combat->hitFlashTime > 0.20f)
        {
            art_.ParticlesPool().SpawnBurst(center, YELLOW, 6, 90.0f, 0.25f);
            damageNumbers_.Spawn({ center.x, unit.position.y - 2.0f },
                                 combat->lastDamageTaken);
            shakeTrauma_ = AddShakeTrauma(shakeTrauma_, kShakeHitTrauma);
            if (unit.teamID == 0)
            {
                pings_.Raise(center, PingKind::UnderAttack);
            }
        }
        if (combat != nullptr && combat->phase == AttackPhase::WindUp)
        {
            if (const Unit *target = registry_.Get<Unit>(combat->target))
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
    registry_.Each<Unit>([&](Entity id, const Unit &unit) {
        (void)unit;
        const CombatState *combat = FindCombatState(registry_, id);
        if (combat != nullptr && combat->phase == AttackPhase::WindUp)
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
        Announce(EventType::ResourceDepleted);
    }
    lastDepletedCount_ = depletedCount;
    UpdateBaseIncome(registry_, resources_, dt, 0);
    nodes_.Update(dt);
    nodes_.GatherTick(registry_, resources_, dt, 0);
    RefreshProducers();
    auto pumpQueue = [&](ProductionQueue &queue, BuildingType type, int &cursor) {
        const Entity producerId = ProducerBuildingAt(registry_, type, 0, cursor);
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
        const cc::IVec2 spawnTile = BuildingSpawnTile(map_, &occ_, *producer);
        const Vector2 spawnPos = cc::ToRaylib(cc::TileToWorld(spawnTile.x, spawnTile.y));
        const Entity spawned = queue.Update(factory_, resources_, 0, spawnPos, dt, &map_, &occ_);
        if (spawned == kInvalidEntity)
        {
            return;
        }
        ++cursor;
        if (Unit *fresh = registry_.Get<Unit>(spawned))
        {
            if (!(cc::WorldToTile(cc::ToGlm(fresh->position)) == rally))
            {
                IssuePathOrderFootprint(*fresh, GetOrders(registry_, spawned),
                                        GetMover(registry_, spawned), map_, occ_,
                                        cc::ToRaylib(cc::TileToWorld(rally.x, rally.y)),
                                        spawned, registry_.Generation(spawned));
            }
            if (autoAddGroupBit_ >= 0)
            {
                fresh->controlGroups |= (1u << static_cast<unsigned int>(autoAddGroupBit_));
            }
        }
    };
    if (hasBootcamp_)
    {
        pumpQueue(bootcampQueue_, BuildingType::Bootcamp, bootcampCursor_);
    }
    if (hasWorkshop_)
    {
        pumpQueue(workshopQueue_, BuildingType::Workshop, workshopCursor_);
    }
    const int queueSize =
        static_cast<int>(bootcampQueue_.Size() + workshopQueue_.Size());
    if (queueSize > lastQueueSize_)
    {
        Announce(EventType::ProductionOrdered);
    }
    lastQueueSize_ = queueSize;
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
    UpdateBuildingConstruction(registry_, dt);
    UpdateBuildingFlash(registry_, dt);
    {
        const Vector2 home = ResolvePlayerRetreatHome(registry_);
        RetreatIfLowHP(registry_, map_, &occ_, home, 0, kRetreatHealthFraction, true);
    }
    if (!sandboxMode_)
    {
        ai_.Update(dt);
    }
    if (worldIs2v2_)
    {
        allyAI_.Update(dt);
        enemyAI2_.Update(dt);
    }
    if (minimap_.PollRefresh(dt))
    {
        BeginTextureMode(minimap_.target);
        ClearBackground(BLACK); // letterbox bars for rectangle maps
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
    if (!sandboxMode_)
    {
        menu_.ShowOutcome(TeamHasUnits(registry_, 0), TeamHasUnits(registry_, 1));
    }
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
