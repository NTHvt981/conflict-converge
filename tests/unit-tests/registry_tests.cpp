// Unit tests for the M1 Goal 6 ECS-lite Registry (lifecycle + component pools).

#include "test_harness.h"

#include "Registry.h"
#include "Unit.h" // real M3 component type

namespace
{

struct Marker
{
    int tag = 0;
};

} // namespace

void RunRegistryTests()
{
    Registry registry;

    // --- creation: unique IDs, sentinel never issued ---
    Entity a = registry.Create();
    Entity b = registry.Create();
    CC_CHECK(a != kInvalidEntity);
    CC_CHECK(b != kInvalidEntity);
    CC_CHECK(a != b);
    CC_CHECK(registry.IsAlive(a));
    CC_CHECK(registry.IsAlive(b));
    CC_CHECK(!registry.IsAlive(kInvalidEntity));
    CC_CHECK(registry.EntityCount() == 2);

    // --- components: add / has / get across coexisting types ---
    Unit unit;
    unit.health = 42.0f;
    unit.type = UnitType::HeavyTank;
    registry.Add(a, unit);
    registry.Add(a, Marker{ 11 });
    registry.Add(b, Marker{ 22 });

    CC_CHECK(registry.Has<Unit>(a));
    CC_CHECK(registry.Has<Marker>(a));
    CC_CHECK(registry.Has<Marker>(b));
    CC_CHECK(!registry.Has<Unit>(b));

    Unit *stored = registry.Get<Unit>(a);
    CC_CHECK(stored != nullptr);
    CC_CHECK(stored->health == 42.0f);
    CC_CHECK(stored->type == UnitType::HeavyTank);

    const Registry &cref = registry;
    CC_CHECK(cref.Get<Unit>(a) != nullptr);
    CC_CHECK(cref.Get<Unit>(b) == nullptr);
    CC_CHECK(cref.Get<Marker>(b)->tag == 22);

    // --- selective remove leaves other types untouched ---
    registry.Remove<Marker>(a);
    CC_CHECK(!registry.Has<Marker>(a));
    CC_CHECK(registry.Has<Unit>(a));
    registry.Remove<Marker>(a); // absent remove is a no-op
    CC_CHECK(registry.Has<Unit>(a));

    // --- destroy strips components; double-destroy is a no-op ---
    registry.Destroy(a);
    CC_CHECK(!registry.IsAlive(a));
    CC_CHECK(!registry.Has<Unit>(a));
    CC_CHECK(registry.EntityCount() == 1);
    registry.Destroy(a);
    CC_CHECK(registry.EntityCount() == 1);
    CC_CHECK(registry.IsAlive(b));
    CC_CHECK(registry.Has<Marker>(b));

    // --- destroyed IDs are recycled and come back clean ---
    Entity recycled = registry.Create();
    CC_CHECK(recycled == a);
    CC_CHECK(registry.IsAlive(recycled));
    CC_CHECK(!registry.Has<Unit>(recycled));
    CC_CHECK(!registry.Has<Marker>(recycled));
    CC_CHECK(registry.EntityCount() == 2);

    // --- generation counter: increments on recycle, stable while alive ---
    CC_CHECK(registry.Generation(a) > 1); // recycled: gen bumped
    const std::uint32_t genRecycled = registry.Generation(recycled);
    CC_CHECK(genRecycled > 1);
    // Create a fresh entity: gen is always > 0 (monotonically increasing)
    Entity fresh = registry.Create();
    CC_CHECK(registry.Generation(fresh) > 0);
    const std::uint32_t genFresh = registry.Generation(fresh);
    // Destroy + re-create: gen bumps again
    registry.Destroy(fresh);
    Entity reCreated = registry.Create();
    CC_CHECK(registry.Generation(reCreated) > genFresh);

    // --- clear resets everything ---
    registry.Clear();
    CC_CHECK(registry.EntityCount() == 0);
    CC_CHECK(!registry.IsAlive(b));
    CC_CHECK(!registry.Has<Marker>(b));
    // Generation resets to 1 after clear
    Entity afterClear = registry.Create();
    CC_CHECK(registry.Generation(afterClear) == 1);
}
