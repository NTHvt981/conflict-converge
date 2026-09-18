// Unit tests for attributes: defaults, field independence, and
// snap preserving the new combat/AI fields.

#include "test_harness.h"

#include "Unit.h"

void RunUnitAttributeTests()
{
    // --- defaults: harmless until the stat table fills them ---
    Unit fresh;
    CC_CHECK(fresh.speed == 0.0f);
    CC_CHECK(fresh.sightRange == 0.0f);
    CC_CHECK(fresh.damageType == DamageType::KINETIC);
    CC_CHECK(fresh.cooldown == 0.0f);
    CC_CHECK(fresh.cooldownTime == 0.0f);
    CC_CHECK(fresh.target == kInvalidEntity);
    CC_CHECK(fresh.state == UnitState::Idle);

    // --- fields are independent and assignable ---
    Unit u;
    u.speed = 96.0f;
    u.sightRange = 320.0f;
    u.damageType = DamageType::EXPLOSIVE;
    u.cooldownTime = 1.5f;
    u.cooldown = 0.75f;
    u.target = 42;
    u.health = 55.0f;
    CC_CHECK(u.speed == 96.0f);
    CC_CHECK(u.sightRange == 320.0f);
    CC_CHECK(u.damageType == DamageType::EXPLOSIVE);
    CC_CHECK(u.cooldownTime == 1.5f);
    CC_CHECK(u.cooldown == 0.75f);
    CC_CHECK(u.target == 42);
    CC_CHECK(u.health == 55.0f);

    // --- snapping touches position only ---
    u.position = { 70.0f, 130.0f };
    SnapUnitToTile(u);
    CC_CHECK(u.position.x == 64.0f);
    CC_CHECK(u.position.y == 128.0f);
    CC_CHECK(u.speed == 96.0f);
    CC_CHECK(u.target == 42);
    CC_CHECK(u.cooldown == 0.75f);
}
