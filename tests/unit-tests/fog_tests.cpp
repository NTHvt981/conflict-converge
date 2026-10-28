// Unit tests for FogOfWar (visibility circles, explored memory,
// team isolation, scout bonus, save/load byte roundtrip).

#include "test_harness.h"

#include "units/Extensions.h"
#include "world/FogOfWar.h"
#include "units/Targeting.h" // Fog-gated acquisition
#include "world/TileMap.h"   // UpdateUnit chase validation under fog

namespace
{

Entity AddWatcher(Registry &registry, TeamId teamID, UnitType type, float sightRange, int tileX,
				  int tileY)
{
	Unit unit;
	unit.teamID = teamID;
	unit.type = type;
	unit.sightRange = sightRange;
	unit.health = 100.0f;
	unit.position = cc::ToRaylib(cc::TileToWorld(tileX, tileY));
	const Entity id = registry.Create();
	registry.Add(id, unit);
	return id;
}

} // namespace

void RunFogTests()
{
	// --- circle reveal: sight 128px = 2 tiles from (5,5) ---
	Registry registry;
	FogOfWar fog;
	fog.Resize(20, 15);
	AddWatcher(registry, kPlayerTeam, UnitType::LightTank, 128.0f, 5, 5);
	fog.Recompute(registry);
	CC_CHECK(fog.IsVisible(kPlayerTeam, { 5, 5 }));
	CC_CHECK(fog.IsVisible(kPlayerTeam, { 7, 5 }));
	CC_CHECK(!fog.IsVisible(kPlayerTeam, { 8, 5 }));
	CC_CHECK(fog.IsVisible(kPlayerTeam, { 6, 6 }));   // 1+1 <= 4: inside the disc
	CC_CHECK(!fog.IsVisible(kPlayerTeam, { 7, 6 }));  // 4+1 > 4: clipped corner
	CC_CHECK(fog.IsExplored(kPlayerTeam, { 7, 5 }));
	CC_CHECK(!fog.IsExplored(kPlayerTeam, { 8, 5 }));

	// --- team isolation: team 1 sees nothing from team 0's eyes ---
	CC_CHECK(!fog.IsVisible(kEnemyTeam, { 5, 5 }));
	CC_CHECK(!fog.IsExplored(kEnemyTeam, { 5, 5 }));

	// --- explored persists after the watcher leaves ---
	registry.Clear();
	AddWatcher(registry, kPlayerTeam, UnitType::LightTank, 128.0f, 15, 12);
	fog.Recompute(registry);
	CC_CHECK(!fog.IsVisible(kPlayerTeam, { 5, 5 }));
	CC_CHECK(fog.IsExplored(kPlayerTeam, { 5, 5 })); // frozen snapshot
	CC_CHECK(fog.IsVisible(kPlayerTeam, { 15, 12 }));

	// --- the blind and the dead reveal nothing ---
	registry.Clear();
	AddWatcher(registry, kPlayerTeam, UnitType::LightTank, 0.0f, 5, 5);
	const Entity corpse = AddWatcher(registry, kPlayerTeam, UnitType::LightTank, 500.0f, 5, 5);
	registry.Get<Unit>(corpse)->health = 0.0f;
	FogOfWar dark;
	dark.Resize(20, 15);
	dark.Recompute(registry);
	CC_CHECK(!dark.IsVisible(kPlayerTeam, { 5, 5 }));
	CC_CHECK(!dark.IsExplored(kPlayerTeam, { 5, 5 }));

	// --- scout bonus: same sightRange, Infantry out-sees LightTank ---
	Registry scoutRegistry;
	FogOfWar scoutFog;
	scoutFog.Resize(20, 15);
	AddWatcher(scoutRegistry, kPlayerTeam, UnitType::LightTank, 64.0f, 5, 5);
	AddWatcher(scoutRegistry, kEnemyTeam, UnitType::RifleInfantry, 64.0f, 15, 5);
	scoutFog.Recompute(scoutRegistry);
	// Radius 1 without bonus: (6,5) yes, (7,5) no. With 1.5x: radius 2.
	CC_CHECK(scoutFog.IsVisible(kPlayerTeam, { 6, 5 }));
	CC_CHECK(!scoutFog.IsVisible(kPlayerTeam, { 7, 5 }));
	CC_CHECK(scoutFog.IsVisible(kEnemyTeam, { 17, 5 })); // scout bonus reaches

	// --- explored byte roundtrip (+ size-mismatch guard) ---
	const std::vector<std::uint8_t> bytes = fog.ExploredBytes(kPlayerTeam);
	CC_CHECK(bytes.size() == 20 * 15);
	FogOfWar restored;
	restored.Resize(20, 15);
	restored.SetExplored(kPlayerTeam, bytes.data(), bytes.size());
	CC_CHECK(restored.IsExplored(kPlayerTeam, { 5, 5 }));
	CC_CHECK(restored.IsExplored(kPlayerTeam, { 15, 12 }));
	CC_CHECK(!restored.IsExplored(kPlayerTeam, { 0, 0 }));
	CC_CHECK(!restored.IsVisible(kPlayerTeam, { 5, 5 })); // bytes carry memory, not sight
	const std::uint8_t shortBytes[4] = { 1, 1, 1, 1 };
	restored.SetExplored(kEnemyTeam, shortBytes, sizeof(shortBytes)); // ignored, kept empty
	CC_CHECK(!restored.IsExplored(kEnemyTeam, { 0, 0 }));
	const std::vector<TeamId> teams = fog.Teams();
	CC_CHECK(teams.size() == 1 && teams[0] == kPlayerTeam);

	// --- resize wipes memory ---
	fog.Resize(20, 15);
	CC_CHECK(!fog.IsExplored(kPlayerTeam, { 5, 5 }));
	CC_CHECK(fog.Teams().empty());
}

void RunFogCombatTests()
{
	// Stale-fog setup: recompute with only the enemy present, then drop the
	// seeker in range WITHOUT recomputing — the enemy tile is unseen by
	// team 0 although inside the seeker's sightRange.
	Registry registry;
	TileMap map(20, 15);
	FogOfWar fog;
	fog.Resize(20, 15);
	Unit foe;
	foe.teamID = kEnemyTeam;
	foe.type = UnitType::LightTank;
	foe.health = 100.0f;
	foe.sightRange = 64.0f;
	foe.position = cc::ToRaylib(cc::TileToWorld(10, 2));
	const Entity foeId = registry.Create();
	registry.Add(foeId, foe);
	fog.Recompute(registry); // team 0 sees nothing; team 1 sees its own ground

	Unit seeker;
	seeker.teamID = kPlayerTeam;
	seeker.type = UnitType::LightTank;
	seeker.health = 100.0f;
	seeker.sightRange = 512.0f; // reaches (10,2): 8 tiles away
	seeker.attackPower = 10;
	seeker.attackRange = 512;
	seeker.cooldownTime = 1.0f;
	seeker.speed = 64.0f;
	seeker.position = cc::ToRaylib(cc::TileToWorld(2, 2));
	const Entity seekerId = registry.Create();
	registry.Add(seekerId, seeker);

	// Ungated legacy path still acquires (default nullptr).
	CC_CHECK(AcquireTarget(registry, seekerId) == foeId);
	// Gated: shrouded tile holds fire.
	CC_CHECK(AcquireTarget(registry, seekerId, &fog) == kInvalidEntity);
	// Fresh recon reveals: recompute with the seeker watching, then acquire.
	fog.Recompute(registry);
	CC_CHECK(AcquireTarget(registry, seekerId, &fog) == foeId);

	// --- artillery blind-fires into shroud at no penalty ---
	Registry artyRegistry;
	FogOfWar artyFog;
	artyFog.Resize(20, 15);
	artyRegistry.Add(artyRegistry.Create(), foe);
	artyFog.Recompute(artyRegistry); // team 0 sees nothing yet
	Unit battery = seeker;
	battery.type = UnitType::Artillery;
	const Entity batteryId = artyRegistry.Create();
	artyRegistry.Add(batteryId, battery);
	CC_CHECK(AcquireTarget(artyRegistry, batteryId, &artyFog) == foeId);

	// --- UpdateUnit under fog: blind units idle, artillery engages ---
	// Fresh registry: foe revealed to nobody, seeker added after recon.
	Registry blindRegistry;
	FogOfWar blindFog;
	blindFog.Resize(20, 15);
	blindRegistry.Add(blindRegistry.Create(), foe);
	blindFog.Recompute(blindRegistry);
	const Entity blindId = blindRegistry.Create();
	blindRegistry.Add(blindId, seeker);
	UpdateUnit(blindId, blindRegistry, map, 1.0f / 60.0f, &blindFog);
	const Unit *blind = blindRegistry.Get<Unit>(blindId);
	CC_CHECK(blindRegistry.Get<CombatState>(blindId)->target == kInvalidEntity);
	CC_CHECK(blind->state == UnitState::Idle);
	UpdateUnit(batteryId, artyRegistry, map, 1.0f / 60.0f, &artyFog);
	const Unit *gun = artyRegistry.Get<Unit>(batteryId);
	CC_CHECK(artyRegistry.Get<CombatState>(batteryId)->target != kInvalidEntity);
}
