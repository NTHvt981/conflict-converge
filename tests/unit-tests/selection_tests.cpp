// Unit tests for M2 Goal 4 selection (pick, exclusive select, deselect).

#include "test_harness.h"

#include "Selection.h"
#include "Unit.h"

namespace
{

Entity SpawnAt(Registry &registry, float x, float y)
{
    Entity entity = registry.Create();
    Unit unit;
    unit.position = { x, y };
    SnapUnitToTile(unit);
    registry.Add(entity, unit);
    return entity;
}

} // namespace

void RunSelectionTests()
{
    Registry registry;
    Entity a = SpawnAt(registry, 64.0f, 64.0f);   // tile (1,1)
    Entity b = SpawnAt(registry, 256.0f, 128.0f); // tile (4,2)

    // --- pick: inside the 64x64 box hits, outside misses ---
    CC_CHECK(PickUnitAt(registry, { 65.0f, 65.0f }) == a);
    CC_CHECK(PickUnitAt(registry, { 127.0f, 127.0f }) == a);
    CC_CHECK(PickUnitAt(registry, { 260.0f, 130.0f }) == b);
    CC_CHECK(PickUnitAt(registry, { 0.0f, 0.0f }) == kInvalidEntity);
    CC_CHECK(PickUnitAt(registry, { 128.0f, 64.0f }) == kInvalidEntity); // box edge is exclusive
    CC_CHECK(PickUnitAt(registry, { 500.0f, 500.0f }) == kInvalidEntity);

    // --- selection is exclusive; empty registry is safe ---
    CC_CHECK(SelectedUnit(registry) == kInvalidEntity);
    SelectOnly(registry, a);
    CC_CHECK(SelectedUnit(registry) == a);
    CC_CHECK(registry.Get<Unit>(a)->isSelected);
    CC_CHECK(!registry.Get<Unit>(b)->isSelected);

    SelectOnly(registry, b);
    CC_CHECK(SelectedUnit(registry) == b);
    CC_CHECK(!registry.Get<Unit>(a)->isSelected);

    DeselectAll(registry);
    CC_CHECK(SelectedUnit(registry) == kInvalidEntity);
    CC_CHECK(!registry.Get<Unit>(b)->isSelected);

    Registry empty;
    CC_CHECK(PickUnitAt(empty, { 65.0f, 65.0f }) == kInvalidEntity);
    CC_CHECK(SelectedUnit(empty) == kInvalidEntity);
    DeselectAll(empty); // no-op, must not crash
    CC_CHECK(true);
}
