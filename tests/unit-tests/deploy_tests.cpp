// Unit tests for the Deployment gate layer: deploying squads are
// untargetable, invulnerable, uncontrollable, and hold no occupancy.

#include "test_harness.h"

#include "app/data/UnitConfig.h"
#include "app/input/Selection.h"
#include "app/ui/GameCamera.h"
#include "economy/Building.h"
#include "units/Combat.h"
#include "units/Deploy.h"
#include "units/Deployment.h"
#include "units/Extensions.h"
#include "units/Targeting.h"
#include "units/Unit.h"
#include "units/UnitInternal.h"

#include <cmath>
#include "units/UnitStats.h"
#include "world/TileMap.h"

namespace
{

Entity SpawnUnit(Registry &registry, UnitType type, TeamId team, float x, float y)
{
	Unit unit;
	unit.type = type;
	ApplyBaseStats(unit);
	unit.teamID = team;
	unit.position = { x, y };
	const Entity id = registry.Create();
	registry.Add(id, unit);
	return id;
}

Entity SpawnTileUnit(Registry &registry, UnitType type, TeamId team, int tileX, int tileY)
{
	return SpawnUnit(registry, type, team, cc::ToRaylib(cc::TileToWorld(tileX, tileY)).x,
					 cc::ToRaylib(cc::TileToWorld(tileX, tileY)).y);
}

} // namespace

void RunDeployTests()
{
	// --- IsDeploying pair predicate: presence and absence of the component ---
	{
		Registry registry;
		const Entity plain = SpawnTileUnit(registry, UnitType::RifleInfantry, kPlayerTeam, 2, 2);
		const Entity deploying =
			SpawnTileUnit(registry, UnitType::RifleInfantry, kPlayerTeam, 3, 2);
		registry.Add(deploying, Deployment{});
		CC_CHECK(!IsDeploying(registry, plain));
		CC_CHECK(IsDeploying(registry, deploying));
	}

	// --- targeting: a deploying squad is never acquired or validated ---
	{
		Registry registry;
		const Entity seeker = SpawnTileUnit(registry, UnitType::RifleInfantry, kPlayerTeam, 1, 1);
		const Entity deploying =
			SpawnTileUnit(registry, UnitType::RifleInfantry, kEnemyTeam, 2, 1);
		registry.Add(deploying, Deployment{});
		CC_CHECK(AcquireTarget(registry, seeker, nullptr, nullptr) == kInvalidEntity);
		CC_CHECK(ValidateTarget(registry, *registry.Get<Unit>(seeker), deploying, nullptr) ==
				 false);
		// A normal enemy next to it is still found and validated.
		const Entity plain = SpawnTileUnit(registry, UnitType::RifleInfantry, kEnemyTeam, 3, 1);
		CC_CHECK(AcquireTarget(registry, seeker, nullptr, nullptr) == plain);
		CC_CHECK(ValidateTarget(registry, *registry.Get<Unit>(seeker), plain, nullptr));
	}

	// --- ResolveAttack no-op: same damage that trivially kills a normal unit ---
	{
		Registry registry;
		Unit attacker;
		attacker.attackPower = 100;
		attacker.damageType = DamageType::KINETIC;
		attacker.cooldownTime = 1.0f;
		CombatState attackerCombat;
		const Entity deployingId = SpawnTileUnit(registry, UnitType::RifleInfantry, kEnemyTeam,
												 2, 2);
		Unit &victim = *registry.Get<Unit>(deployingId);
		const float before = victim.health;
		registry.Add(deployingId, Deployment{});
		CombatState &victimCombat = GetCombatState(registry, deployingId);
		CC_CHECK(ResolveAttack(registry, deployingId, attacker, attackerCombat, victim,
							   victimCombat) == 0.0f);
		CC_CHECK(victim.health == before);
		CC_CHECK(victimCombat.lastDamageTaken == 0.0f);
		CC_CHECK(victimCombat.hitFlashTime == 0.0f);
	}

	// --- splash: ResolveGroundAttack never picks a deploying victim ---
	{
		Registry registry;
		const Entity attacker =
			SpawnTileUnit(registry, UnitType::AntiArmorInfantry, kPlayerTeam, 1, 1);
		const Entity deploying =
			SpawnTileUnit(registry, UnitType::RifleInfantry, kEnemyTeam, 2, 1);
		registry.Add(deploying, Deployment{});
		Unit &victim = *registry.Get<Unit>(deploying);
		const float before = victim.health;
		ResolveGroundAttack(registry, *registry.Get<Unit>(attacker),
							GetCombatState(registry, attacker),
							{ victim.position.x + cc::kHalfTile,
							  victim.position.y + cc::kHalfTile });
		CC_CHECK(registry.Get<Unit>(deploying)->health == before);
	}

	// --- arcing shell: flight-time impact skips a deploying unit ---
	{
		ResetActiveUnitConfigs(); // Artillery arcs by default (0.6s, splash 1)
		Registry registry;
		TileMap map(20, 15);
		const Entity id = SpawnTileUnit(registry, UnitType::Artillery, kPlayerTeam, 1, 1);
		Unit &gun = *registry.Get<Unit>(id);
		const Entity deploying =
			SpawnTileUnit(registry, UnitType::RifleInfantry, kEnemyTeam, 3, 1);
		registry.Add(deploying, Deployment{});
		const float before = registry.Get<Unit>(deploying)->health;
		ResolveStrike(registry, id, gun, GetCombatState(registry, id), deploying);
		UpdateProjectiles(registry, map, 1.0f); // 0.6s flight resolves over the victim
		CC_CHECK(registry.Get<Unit>(deploying)->health == before);
		ResetActiveUnitConfigs();
	}

	// --- selection: deploying squads never end up selected ---
	{
		Registry registry;
		const Entity deploying =
			SpawnTileUnit(registry, UnitType::RifleInfantry, kPlayerTeam, 2, 2);
		const Entity plain = SpawnTileUnit(registry, UnitType::RifleInfantry, kPlayerTeam, 2, 3);
		registry.Add(deploying, Deployment{});

		SelectOnly(registry, deploying);
		CC_CHECK(!registry.Get<Unit>(deploying)->isSelected);
		CC_CHECK(SelectedUnitCount(registry) == 0);

		SelectOnly(registry, plain);
		CC_CHECK(registry.Get<Unit>(plain)->isSelected);

		// Box over both: only the non-deploying unit is picked.
		DeselectAll(registry);
		const Rectangle box = { cc::ToRaylib(cc::TileToWorld(1, 1)).x,
								cc::ToRaylib(cc::TileToWorld(1, 1)).y, 5.0f * 64.0f,
								5.0f * 64.0f };
		CC_CHECK(SelectInRect(registry, box, false) == 1);
		CC_CHECK(!registry.Get<Unit>(deploying)->isSelected);
		CC_CHECK(registry.Get<Unit>(plain)->isSelected);

		// Control-group recall also drops a deploying member.
		registry.Get<Unit>(plain)->controlGroups = 1u << 3;
		registry.Get<Unit>(deploying)->controlGroups = 1u << 3;
		DeselectAll(registry);
		CC_CHECK(RecallControlGroup(registry, 3) == 1);
		CC_CHECK(registry.Get<Unit>(plain)->isSelected);
		CC_CHECK(!registry.Get<Unit>(deploying)->isSelected);

		// Click pick also skips the door squad.
		DeselectAll(registry);
		const Vector2 spot = cc::ToRaylib(cc::TileToWorld(2, 2));
		const Vector2 center = { spot.x + cc::kHalfTile, spot.y + cc::kHalfTile };
		CC_CHECK(PickUnitAt(registry, center) == kInvalidEntity);
	}

	// --- occupancy: a deploying squad reserves no footprint tiles ---
	{
		Registry registry;
		TileMap map(20, 15);
		OccupancyGrid occ(20, 15);
		const Entity deploying =
			SpawnTileUnit(registry, UnitType::LightTank, kPlayerTeam, 5, 5);
		registry.Add(deploying, Deployment{});
		RunUnitMovementFrame(registry, map, occ, nullptr, 1.0f / 60.0f);
		const cc::IVec2 tile = cc::WorldToTile(cc::ToGlm(registry.Get<Unit>(deploying)->position));
		CC_CHECK(occ.GetUnit(tile).entity == kOccEmpty);
		// A plain unit of the same shape still reserves its tile.
		const Entity plain = SpawnTileUnit(registry, UnitType::LightTank, kPlayerTeam, 8, 5);
		(void)plain;
		RunUnitMovementFrame(registry, map, occ, nullptr, 1.0f / 60.0f);
		const cc::IVec2 plainTile =
			cc::WorldToTile(cc::ToGlm(registry.Get<Unit>(plain)->position));
		CC_CHECK(occ.GetUnit(plainTile).entity == plain);
	}

	// --- no regression: an ordinary squad is targetable, damageable, selectable ---
	{
		Registry registry;
		const Entity seeker = SpawnTileUnit(registry, UnitType::RifleInfantry, kPlayerTeam, 1, 1);
		const Entity plain = SpawnTileUnit(registry, UnitType::RifleInfantry, kEnemyTeam, 2, 1);
		CC_CHECK(AcquireTarget(registry, seeker, nullptr, nullptr) == plain);
		Unit &attacker = *registry.Get<Unit>(seeker);
		CombatState &attackerCombat = GetCombatState(registry, seeker);
		Unit &victim = *registry.Get<Unit>(plain);
		const float before = victim.health;
		CC_CHECK(ResolveAttack(registry, plain, attacker, attackerCombat, victim,
							   GetCombatState(registry, plain)) > 0.0f);
		CC_CHECK(victim.health < before);
		SelectOnly(registry, plain);
		CC_CHECK(registry.Get<Unit>(plain)->isSelected);
	}

	// --- BeginDeploy: door placement, member budget, no reserve, no orders ---
	{
		Registry registry;
		TileMap map(20, 15);
		OccupancyGrid occ(20, 15);
		const Entity bootcamp =
			PlaceBuilding(registry, map, BuildingType::Bootcamp, kPlayerTeam, 2, 2);
		CC_CHECK(bootcamp != kInvalidEntity);
		const Entity unitId =
			SpawnTileUnit(registry, UnitType::RifleInfantry, kPlayerTeam, 8, 8);
		BeginDeploy(registry, unitId, bootcamp, map, &occ, 2);

		CC_CHECK(IsDeploying(registry, unitId));
		const Deployment *deployment = registry.Get<Deployment>(unitId);
		CC_CHECK(deployment != nullptr);
		CC_CHECK(deployment->membersTotal == 5);
		const cc::IVec2 door =
			BuildingSpawnTile(map, &occ, *registry.Get<Building>(bootcamp));
		CC_CHECK(cc::WorldToTile(cc::ToGlm(registry.Get<Unit>(unitId)->position)) == door);
		CC_CHECK(occ.GetUnit(door).entity == kOccEmpty);
		CC_CHECK(!registry.Has<Mover>(unitId) || (!registry.Get<Mover>(unitId)->hasMoveOrder &&
												  !registry.Get<Mover>(unitId)->hasPath));
		CC_CHECK(!registry.Has<Orders>(unitId) || !registry.Get<Orders>(unitId)->attackMove);
		CC_CHECK(registry.Get<Unit>(unitId)->controlGroups == 0);

		const Entity seeker =
			SpawnTileUnit(registry, UnitType::RifleInfantry, kEnemyTeam, door.x + 2, door.y);
		CC_CHECK(AcquireTarget(registry, seeker, nullptr, nullptr) != unitId);
	}

	// --- UpdateDeploy: staggered exit, slot walk, assembly, rally, group bit ---
	{
		Registry registry;
		TileMap map(20, 15);
		OccupancyGrid occ(20, 15);
		const Entity bootcamp =
			PlaceBuilding(registry, map, BuildingType::Bootcamp, kPlayerTeam, 2, 2);
		registry.Get<Building>(bootcamp)->rallyTile = { 10, 12 };
		const Entity unitId =
			SpawnTileUnit(registry, UnitType::RifleInfantry, kPlayerTeam, 8, 8);
		BeginDeploy(registry, unitId, bootcamp, map, &occ, 2);
		const cc::IVec2 door =
			BuildingSpawnTile(map, &occ, *registry.Get<Building>(bootcamp));

		constexpr float kDt = 1.0f / 60.0f;
		const float delay = kDeployExitDelayPerMember;
		// The k-th member exits when cumulative time crosses k * delay;
		// ceil gives that tick exactly, and any tick before it is safely
		// below the threshold.
		const auto ticksToSpawn = [&](float member) {
			return static_cast<int>(std::ceil(member * delay / kDt));
		};
		const int firstExit = ticksToSpawn(1);
		const auto run = [&](int ticks) {
			for (int i = 0; i < ticks; ++i)
			{
				UpdateDeploy(registry, map, &occ, kDt);
			}
		};
		run(firstExit - 1);
		CC_CHECK(registry.Get<Deployment>(unitId)->membersSpawned == 0);
		run(1);
		CC_CHECK(registry.Get<Deployment>(unitId)->membersSpawned == 1);
		run(ticksToSpawn(2) - firstExit - 1);
		CC_CHECK(registry.Get<Deployment>(unitId)->membersSpawned == 1);
		run(1);
		CC_CHECK(registry.Get<Deployment>(unitId)->membersSpawned == 2);
		const cc::Vec2 walking = registry.Get<Deployment>(unitId)->memberSpawnedPos[1];
		run(1);
		CC_CHECK(!(registry.Get<Deployment>(unitId)->memberSpawnedPos[1] == walking));

		for (int i = 0; i < 1200 && IsDeploying(registry, unitId); ++i)
		{
			UpdateDeploy(registry, map, &occ, kDt);
		}
		CC_CHECK(!IsDeploying(registry, unitId));
		CC_CHECK(cc::WorldToTile(cc::ToGlm(registry.Get<Unit>(unitId)->position)) == door);
		CC_CHECK(occ.GetUnit(door).entity == unitId);
		const Mover *mover = registry.Get<Mover>(unitId);
		CC_CHECK(mover != nullptr && (mover->hasMoveOrder || mover->hasPath));
		CC_CHECK((registry.Get<Unit>(unitId)->controlGroups & (1u << 2)) != 0);

		// --- post-assembly: targetable and damageable like any other squad ---
		const Entity seeker = SpawnTileUnit(registry, UnitType::RifleInfantry, kEnemyTeam,
											door.x + 2, door.y);
		CC_CHECK(AcquireTarget(registry, seeker, nullptr, nullptr) == unitId);
		Unit &attacker = *registry.Get<Unit>(seeker);
		attacker.attackPower = 7;
		attacker.damageType = DamageType::KINETIC;
		attacker.cooldownTime = 1.0f;
		Unit &victim = *registry.Get<Unit>(unitId);
		const float before = victim.health;
		CC_CHECK(ResolveAttack(registry, unitId, attacker, GetCombatState(registry, seeker),
							   victim, GetCombatState(registry, unitId)) > 0.0f);
		CC_CHECK(victim.health < before);
	}

	// --- determinism: cumulative time alone drives the exit cadence ---
	{
		const auto spawnedAfter = [](float step, int ticks) {
			Registry registry;
			TileMap map(20, 15);
			OccupancyGrid occ(20, 15);
			const Entity bootcamp =
				PlaceBuilding(registry, map, BuildingType::Bootcamp, kPlayerTeam, 2, 2);
			const Entity unitId =
				SpawnTileUnit(registry, UnitType::RifleInfantry, kPlayerTeam, 8, 8);
			BeginDeploy(registry, unitId, bootcamp, map, &occ, -1);
			for (int i = 0; i < ticks; ++i)
			{
				UpdateDeploy(registry, map, &occ, step);
			}
			const Deployment *deployment = registry.Get<Deployment>(unitId);
			return deployment == nullptr ? -1 : deployment->membersSpawned;
		};
		// Same total time, quantized differently: splitting the walk into
		// pieces must not change how many members have exited. Both totals
		// (1.5x and 0.75x of the stagger) sit clear of any threshold.
		const int coarse = spawnedAfter(kDeployExitDelayPerMember * 0.5f, 3);
		const int fine = spawnedAfter(kDeployExitDelayPerMember * 0.25f, 6);
		CC_CHECK(coarse == 1);
		CC_CHECK(coarse == fine);
	}
}
