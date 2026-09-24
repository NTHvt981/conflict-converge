// Unit tests for context-sensitive cursor intent prediction (Cursor.h).
// Pure function, no window: hovering enemies/repair patients/heal patients/
// ground maps to Attack/Repair/Heal/Move; power-0 support types never offer
// Attack; empty selection always yields Default.

#include "test_harness.h"

#include "Cursor.h"
#include "Extensions.h"
#include "Unit.h"
#include "UnitStats.h"

namespace
{

Entity SpawnUnit(Registry &registry, UnitType type, int team, float x, float y, float healthFrac = 1.0f)
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
    return { unit->position.x + 32.0f, unit->position.y + 32.0f };
}

} // namespace

void RunCursorTests()
{
    Registry registry;
    Entity attacker = SpawnUnit(registry, UnitType::RifleInfantry, 0, 64.0f, 64.0f);
    Entity enemy = SpawnUnit(registry, UnitType::RifleInfantry, 1, 256.0f, 128.0f);
    Entity engineer = SpawnUnit(registry, UnitType::Engineer, 0, 64.0f, 320.0f);
    Entity wounded = SpawnUnit(registry, UnitType::LightTank, 0, 320.0f, 320.0f, 0.5f);
    Entity healthy = SpawnUnit(registry, UnitType::LightTank, 0, 448.0f, 320.0f);
    Entity enemyTank = SpawnUnit(registry, UnitType::LightTank, 1, 512.0f, 320.0f, 0.5f);
    Entity medic = SpawnUnit(registry, UnitType::Medic, 0, 64.0f, 448.0f);
    Entity hurtFoot = SpawnUnit(registry, UnitType::RifleInfantry, 0, 320.0f, 448.0f, 0.5f);
    Entity wholeFoot = SpawnUnit(registry, UnitType::RifleInfantry, 0, 448.0f, 448.0f);
    Entity foeFoot = SpawnUnit(registry, UnitType::RifleInfantry, 1, 576.0f, 448.0f, 0.5f);
    const Unit *sel = registry.Get<Unit>(attacker);
    const Unit *eng = registry.Get<Unit>(engineer);
    const Unit *med = registry.Get<Unit>(medic);

    // --- empty selection never predicts an order cursor ---
    CC_CHECK(PredictCursorIntent(registry, nullptr, UnitCenter(registry, enemy)) ==
             CursorIntent::Default);
    CC_CHECK(PredictCursorIntent(registry, nullptr, { 800.0f, 800.0f }) == CursorIntent::Default);

    // --- enemy unit under an attack-capable selection: Attack ---
    CC_CHECK(PredictCursorIntent(registry, sel, UnitCenter(registry, enemy)) ==
             CursorIntent::Attack);

    // --- empty ground: Move ---
    CC_CHECK(PredictCursorIntent(registry, sel, { 800.0f, 800.0f }) == CursorIntent::Move);

    // --- friendly unit that needs nothing: Move (no repair, no attack) ---
    CC_CHECK(PredictCursorIntent(registry, sel, UnitCenter(registry, healthy)) ==
             CursorIntent::Move);

    // --- non-Engineer over a damaged friendly vehicle: still Move ---
    CC_CHECK(PredictCursorIntent(registry, sel, UnitCenter(registry, wounded)) ==
             CursorIntent::Move);

    // --- Engineer over a damaged friendly vehicle: Repair ---
    CC_CHECK(PredictCursorIntent(registry, eng, UnitCenter(registry, wounded)) ==
             CursorIntent::Repair);

    // --- Engineer over a healthy friendly vehicle: Move ---
    CC_CHECK(PredictCursorIntent(registry, eng, UnitCenter(registry, healthy)) ==
             CursorIntent::Move);

    // --- Engineer over a damaged ENEMY vehicle: Move (support cannot attack) ---
    CC_CHECK(PredictCursorIntent(registry, eng, UnitCenter(registry, enemyTank)) ==
             CursorIntent::Move);

    // --- Medic over a wounded friendly squadmate: Heal ---
    CC_CHECK(PredictCursorIntent(registry, med, UnitCenter(registry, hurtFoot)) ==
             CursorIntent::Heal);

    // --- Medic over healthy flesh, vehicles, or enemies: Move ---
    CC_CHECK(PredictCursorIntent(registry, med, UnitCenter(registry, wholeFoot)) ==
             CursorIntent::Move);
    CC_CHECK(PredictCursorIntent(registry, med, UnitCenter(registry, wounded)) ==
             CursorIntent::Move);
    CC_CHECK(PredictCursorIntent(registry, med, UnitCenter(registry, foeFoot)) ==
             CursorIntent::Move);

    // --- non-Medic over wounded flesh: still Move ---
    CC_CHECK(PredictCursorIntent(registry, sel, UnitCenter(registry, hurtFoot)) ==
             CursorIntent::Move);
    CC_CHECK(PredictCursorIntent(registry, eng, UnitCenter(registry, hurtFoot)) ==
             CursorIntent::Move);

    // --- G4 carrier over friendly foot: Load; enemy still wins as Attack ---
    Entity carrier = SpawnUnit(registry, UnitType::IFV, 0, 64.0f, 576.0f);
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

    // --- full carrier: back to Move ---
    for (int i = 0; i < 4; ++i)
    {
        const Entity seat =
            SpawnUnit(registry, UnitType::RifleInfantry, 0, 700.0f + i * 64.0f, 576.0f);
        EmbarkedOn ride;
        ride.carrier = carrier;
        registry.Add(seat, ride);
        registry.Get<Cargo>(carrier)->passengers.push_back(seat);
    }
    drv = registry.Get<Unit>(carrier); // re-fetch: seat spawns may rehash
    CC_CHECK(PredictCursorIntent(registry, drv, UnitCenter(registry, wholeFoot)) ==
             CursorIntent::Move);
}
