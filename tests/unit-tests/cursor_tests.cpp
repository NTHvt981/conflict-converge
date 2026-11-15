// Unit tests for context-sensitive cursor intent prediction (Cursor.h).
// Pure function, no window needed.

#include "test_harness.h"

#include "app/ui/Cursor.h"
#include "units/Extensions.h"
#include "units/Unit.h"
#include "units/UnitStats.h"

namespace
{

Entity SpawnUnit(Registry &registry, UnitType type, TeamId team, float x, float y,
				 float healthFrac = 1.0f)
{
	Entity entity = registry.Create();
	Unit unit;
	unit.type = type;
	unit.teamID = team;
	unit.position = { x, y };
	SnapUnitToTile(unit);
	unit.health = BaseStats(type).health * healthFrac;
	registry.Add(entity, unit);
	return entity;
}

Vector2 UnitCenter(const Registry &registry, Entity id)
{
	const Unit *unit = registry.Get<Unit>(id);
	return { unit->position.x + cc::kHalfTile, unit->position.y + cc::kHalfTile };
}

} // namespace

void RunCursorTests()
{
	Registry registry;
	Entity attacker = SpawnUnit(registry, UnitType::RifleInfantry, kPlayerTeam, 64.0f, 64.0f);
	Entity enemy = SpawnUnit(registry, UnitType::RifleInfantry, kEnemyTeam, 256.0f, 128.0f);
	Entity engineer = SpawnUnit(registry, UnitType::Engineer, kPlayerTeam, 64.0f, 320.0f);
	Entity wounded = SpawnUnit(registry, UnitType::LightTank, kPlayerTeam, 320.0f, 320.0f, 0.5f);
	Entity healthy = SpawnUnit(registry, UnitType::LightTank, kPlayerTeam, 448.0f, 320.0f);
	Entity enemyTank = SpawnUnit(registry, UnitType::LightTank, kEnemyTeam, 512.0f, 320.0f, 0.5f);
	Entity medic = SpawnUnit(registry, UnitType::Medic, kPlayerTeam, 64.0f, 448.0f);
	Entity hurtFoot =
		SpawnUnit(registry, UnitType::RifleInfantry, kPlayerTeam, 320.0f, 448.0f, 0.5f);
	Entity wholeFoot = SpawnUnit(registry, UnitType::RifleInfantry, kPlayerTeam, 448.0f, 448.0f);
	Entity foeFoot = SpawnUnit(registry, UnitType::RifleInfantry, kEnemyTeam, 576.0f, 448.0f, 0.5f);
	const Unit *sel = registry.Get<Unit>(attacker);
	const Unit *eng = registry.Get<Unit>(engineer);
	const Unit *med = registry.Get<Unit>(medic);

	CC_CHECK(PredictCursorIntent(registry, nullptr, UnitCenter(registry, enemy)) ==
			 CursorIntent::Default);
	CC_CHECK(PredictCursorIntent(registry, nullptr, { 800.0f, 800.0f }) == CursorIntent::Default);

	CC_CHECK(PredictCursorIntent(registry, sel, UnitCenter(registry, enemy)) ==
			 CursorIntent::Attack);

	CC_CHECK(PredictCursorIntent(registry, sel, { 800.0f, 800.0f }) == CursorIntent::Move);

	CC_CHECK(PredictCursorIntent(registry, sel, UnitCenter(registry, healthy)) ==
			 CursorIntent::Move);

	CC_CHECK(PredictCursorIntent(registry, sel, UnitCenter(registry, wounded)) ==
			 CursorIntent::Move);

	CC_CHECK(PredictCursorIntent(registry, eng, UnitCenter(registry, wounded)) ==
			 CursorIntent::Repair);

	CC_CHECK(PredictCursorIntent(registry, eng, UnitCenter(registry, healthy)) ==
			 CursorIntent::Move);

	CC_CHECK(PredictCursorIntent(registry, eng, UnitCenter(registry, enemyTank)) ==
			 CursorIntent::Move);

	CC_CHECK(PredictCursorIntent(registry, med, UnitCenter(registry, hurtFoot)) ==
			 CursorIntent::Heal);

	CC_CHECK(PredictCursorIntent(registry, med, UnitCenter(registry, wholeFoot)) ==
			 CursorIntent::Move);
	CC_CHECK(PredictCursorIntent(registry, med, UnitCenter(registry, wounded)) ==
			 CursorIntent::Move);
	CC_CHECK(PredictCursorIntent(registry, med, UnitCenter(registry, foeFoot)) ==
			 CursorIntent::Move);

	CC_CHECK(PredictCursorIntent(registry, sel, UnitCenter(registry, hurtFoot)) ==
			 CursorIntent::Move);
	CC_CHECK(PredictCursorIntent(registry, eng, UnitCenter(registry, hurtFoot)) ==
			 CursorIntent::Move);

	Entity carrier = SpawnUnit(registry, UnitType::IFV, kPlayerTeam, 64.0f, 576.0f);
	Cargo cargo;
	cargo.capacity = 4;
	registry.Add(carrier, cargo);
	const Unit *drv = registry.Get<Unit>(carrier);
	CC_CHECK(PredictCursorIntent(registry, drv, UnitCenter(registry, wholeFoot)) ==
			 CursorIntent::Load);
	CC_CHECK(PredictCursorIntent(registry, drv, UnitCenter(registry, foeFoot)) ==
			 CursorIntent::Attack); // IFV can attack: enemy wins over load
	CC_CHECK(PredictCursorIntent(registry, drv, UnitCenter(registry, wounded)) ==
			 CursorIntent::Move); // vehicles are not passengers

	for (int i = 0; i < 4; ++i)
	{
		const Entity seat =
			SpawnUnit(registry, UnitType::RifleInfantry, kPlayerTeam, 700.0f + i * 64.0f, 576.0f);
		EmbarkedOn ride;
		ride.carrier = carrier;
		registry.Add(seat, ride);
		registry.Get<Cargo>(carrier)->passengers.push_back(seat);
	}
	drv = registry.Get<Unit>(carrier); // re-fetch: seat spawns may rehash
	CC_CHECK(PredictCursorIntent(registry, drv, UnitCenter(registry, wholeFoot)) ==
			 CursorIntent::Move);
}
