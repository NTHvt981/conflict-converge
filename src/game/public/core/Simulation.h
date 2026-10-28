#pragma once

#include "units/AICommander.h"
#include "app/data/Art.h"
#include "app/data/Audio.h"
#include "core/Event.h"
#include "world/FogOfWar.h"
#include "app/match/Menu.h"
#include "app/ui/Minimap.h"
#include "economy/Building.h"
#include "economy/Nodes.h"
#include "app/ui/Pings.h"
#include "economy/Production.h"
#include "core/Registry.h"
#include "economy/ResourceSystem.h"
#include "app/save/SaveGame.h"
#include "core/Subsystem.h"
#include "world/TileMap.h"
#include "units/UnitFactory.h"

class PlayingInput;

// It polls no input: the one player state it reads is the auto-add control-group
// bit, at spawn. It refreshes the minimap texture and gates the match outcome,
// so unit tests can drive Step headlessly over fixture worlds.
class Simulation
{
public:
	Simulation(Subsystems &world, Subsystems &engine, MenuFlow &menu,
			   const WorldState &worldState, DamageNumbers &damageNumbers,
			   const PlayingInput &input,
				const bool &sandboxMode, const bool &worldIs2v2,
				float &shakeTrauma, MenuState &lastOutcomeState);
	Simulation(const Simulation &) = delete;
	Simulation &operator=(const Simulation &) = delete;

	void ResetForMatch();
	void ResetEdgePolls();
	void StopRecording();
	void SetReplayCount(int count);
	int ReplayCount() const;
	// Re-runnable for the paused HUD.
	void RefreshProducers();
	bool HasBootcamp() const;
	bool HasWorkshop() const;
	// Runs only while Playing.
	void Step(float dt);

private:
	void Announce(EventType type);
	void ReapDeadUnits();
	void UpdateCombatVfx();
	void UpdateParticlePools(float dt);
	void UpdateAttackSfx(float dt);
	void AnnounceEconomyChanges();
	void StepEconomy(float dt);
	void PumpProduction(float dt);
	void PumpQueue(ProductionQueue &queue, BuildingType type, int &cursor, float dt);
	void RecordReplayFrame(float dt);
	void UpdateStructures(float dt);
	void UpdateRetreat();
	void UpdateCommanders(float dt);
	void RefreshMinimapTexture(float dt);
	void UpdateOutcome();

	Registry &registry_;
	TileMap &map_;
	OccupancyGrid &occ_;
	FogOfWar &fog_;
	ResourceNodes &nodes_;
	ProductionQueue &bootcampQueue_;
	ProductionQueue &workshopQueue_;
	UnitFactory &factory_;
	ResourceSystem &resources_;
	AICommander &ai_;
	AICommander &allyAI_;
	AICommander &enemyAI2_;
	Art &art_;
	Audio &audio_;
	Pings &pings_;
	MenuFlow &menu_;
	Minimap &minimap_;
	const WorldState &worldState_;
	DamageNumbers &damageNumbers_;
	EventDispatcher &events_;
	const PlayingInput &input_;
	const bool &sandboxMode_;
	const bool &worldIs2v2_;
	float &shakeTrauma_;
	MenuState &lastOutcomeState_;
	int lastBuildingCount_ = 0;
	int lastDepletedCount_ = 0;
	int lastQueueSize_ = 0;
	float attackSfxTimer_ = 0.0f;
	bool hasBootcamp_ = false;
	bool hasWorkshop_ = false;
	int bootcampCursor_ = 0;
	int workshopCursor_ = 0;
	bool replayRecording_ = false;
	float replayTimer_ = 0.0f;
	int replayIndex_ = 0;
	int replayCount_ = 0;
};
