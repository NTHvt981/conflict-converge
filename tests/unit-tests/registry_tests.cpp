// Unit tests for the ECS-lite Registry (lifecycle + component pools).

#include "test_harness.h"

#include "core/Registry.h"
#include "units/Unit.h" // real component type

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

	// --- any: first-match predicate scan with early stop ---
	Registry anyRegistry;
	CC_CHECK(!anyRegistry.Any<Marker>([](Entity, const Marker &) { return true; }));
	anyRegistry.Create();
	anyRegistry.Create();
	anyRegistry.Add(anyRegistry.Create(), Marker{ 11 });
	anyRegistry.Add(anyRegistry.Create(), Marker{ 22 });
	anyRegistry.Add(anyRegistry.Create(), Marker{ 33 });

	CC_CHECK(!anyRegistry.Any<Marker>([](Entity, const Marker &m) { return m.tag == 99; }));
	int seenAll = 0;
	CC_CHECK(anyRegistry.Any<Marker>([&](Entity, const Marker &m) {
		++seenAll;
		return m.tag == 33;
	}));
	CC_CHECK(seenAll <= 3);
	int seenStop = 0;
	CC_CHECK(anyRegistry.Any<Marker>([&](Entity, const Marker &) {
		++seenStop;
		return true;
	}));
	CC_CHECK(seenStop == 1);
	int seenMiss = 0;
	CC_CHECK(!anyRegistry.Any<Marker>([&](Entity, const Marker &) {
		++seenMiss;
		return false;
	}));
	CC_CHECK(seenMiss == 3);

	const Registry &anyView = anyRegistry;
	CC_CHECK(anyView.Any<Marker>([](Entity, const Marker &m) { return m.tag == 22; }));
	CC_CHECK(!anyView.Any<Marker>([](Entity, const Marker &m) { return m.tag == 99; }));

	// --- each/any iterate entity ids ascending regardless of insertion order ---
	Registry orderRegistry;
	Entity oa = orderRegistry.Create();
	Entity ob = orderRegistry.Create();
	Entity oc = orderRegistry.Create();
	orderRegistry.Add(oc, Marker{ 3 }); // insert descending: 3, 1, 2
	orderRegistry.Add(oa, Marker{ 1 });
	orderRegistry.Add(ob, Marker{ 2 });

	std::vector<Entity> eachOrder;
	orderRegistry.Each<Marker>([&](Entity id, const Marker &) { eachOrder.push_back(id); });
	CC_CHECK(eachOrder.size() == 3);
	CC_CHECK(eachOrder[0] == oa && eachOrder[1] == ob && eachOrder[2] == oc);

	const Registry &orderView = orderRegistry;
	std::vector<Entity> eachConstOrder;
	orderView.Each<Marker>([&](Entity id, const Marker &) { eachConstOrder.push_back(id); });
	CC_CHECK(eachConstOrder.size() == 3);
	CC_CHECK(eachConstOrder[0] == oa && eachConstOrder[1] == ob && eachConstOrder[2] == oc);

	// First match of a always-true predicate is the lowest id, not insertion order.
	Entity anyFirst = kInvalidEntity;
	orderRegistry.Any<Marker>([&](Entity id, const Marker &) {
		anyFirst = id;
		return true;
	});
	CC_CHECK(anyFirst == oa);

	// Erase-out-of-middle keeps the surviving ids ascending.
	orderRegistry.Remove<Marker>(ob);
	eachOrder.clear();
	orderRegistry.Each<Marker>([&](Entity id, const Marker &) { eachOrder.push_back(id); });
	CC_CHECK(eachOrder.size() == 2);
	CC_CHECK(eachOrder[0] == oa && eachOrder[1] == oc);
}
