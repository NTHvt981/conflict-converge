// Unit tests for attributes: defaults, field independence, and
// snap preserving the new combat/AI fields.

#include "test_harness.h"

#include "units/Extensions.h"
#include "units/Unit.h"

void RunUnitAttributeTests()
{
    // --- defaults: harmless until the stat table fills them ---
    Unit fresh;
    CC_CHECK(fresh.speed == 0.0f);
    CC_CHECK(fresh.sightRange == 0.0f);
    CC_CHECK(fresh.damageType == DamageType::KINETIC);
    CC_CHECK(fresh.cooldownTime == 0.0f);
    CC_CHECK(fresh.state == UnitState::Idle);
    CombatState freshCombat;
    CC_CHECK(freshCombat.cooldown == 0.0f);
    CC_CHECK(freshCombat.target == kInvalidEntity);

    // --- fields are independent and assignable ---
    Unit u;
    u.speed = 96.0f;
    u.sightRange = 320.0f;
    u.damageType = DamageType::EXPLOSIVE;
    u.cooldownTime = 1.5f;
    u.health = 55.0f;
    CombatState c;
    c.cooldown = 0.75f;
    c.target = 42;
    CC_CHECK(u.speed == 96.0f);
    CC_CHECK(u.sightRange == 320.0f);
    CC_CHECK(u.damageType == DamageType::EXPLOSIVE);
    CC_CHECK(u.cooldownTime == 1.5f);
    CC_CHECK(c.cooldown == 0.75f);
    CC_CHECK(c.target == 42);
    CC_CHECK(u.health == 55.0f);

    // --- snapping touches position only ---
    u.position = { 70.0f, 130.0f };
    SnapUnitToTile(u);
    CC_CHECK(u.position.x == 64.0f);
    CC_CHECK(u.position.y == 128.0f);
    CC_CHECK(u.speed == 96.0f);
    CC_CHECK(c.target == 42);
    CC_CHECK(c.cooldown == 0.75f);
}
