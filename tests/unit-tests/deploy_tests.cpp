// Unit tests for the Deployment gate layer: deploying squads are
// untargetable, invulnerable, uncontrollable, and hold no occupancy.

#include "test_harness.h"

#include "app/data/UnitConfig.h"
#include "app/input/Selection.h"
#include "app/ui/GameCamera.h"
#include "units/Combat.h"
#include "units/Deployment.h"
#include "units/Extensions.h"
#include "units/Targeting.h"
#include "units/Unit.h"
#include "units/UnitInternal.h"
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
}
