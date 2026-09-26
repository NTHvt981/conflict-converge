// Unit tests for selection (pick, exclusive select, deselect).

#include "test_harness.h"

#include "units/Extensions.h"
#include "app/ui/GameCamera.h"
#include "app/input/Selection.h"
#include "units/Unit.h"

namespace
{

Entity SpawnAt(Registry &registry, float x, float y)
{
    Entity entity = registry.Create();
    Unit unit;
    unit.position = { x, y };
    SnapUnitToTile(unit);
    registry.Add(entity, unit);
    registry.Add(entity, Orders{});
    registry.Add(entity, Mover{});
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

    // --- SelectedUnitCount: 0 / N / mixed selected+unselected ---
    CC_CHECK(SelectedUnitCount(registry) == 0);
    SelectOnly(registry, a);
    CC_CHECK(SelectedUnitCount(registry) == 1);
    registry.Get<Unit>(b)->isSelected = true; // extend without clearing a
    CC_CHECK(SelectedUnitCount(registry) == 2);
    Entity c = SpawnAt(registry, 400.0f, 400.0f);
    CC_CHECK(SelectedUnitCount(registry) == 2); // unselected c not counted
    (void)c;
    CC_CHECK(SelectedUnitCount(empty) == 0);

    // --- NormalizeRect: either corner may lead ---
    const Rectangle norm = NormalizeRect({ 5.0f, 9.0f }, { 1.0f, 3.0f });
    CC_CHECK(norm.x == 1.0f && norm.y == 3.0f);
    CC_CHECK(norm.width == 4.0f && norm.height == 6.0f);
    const Rectangle same = NormalizeRect({ 2.0f, 2.0f }, { 2.0f, 2.0f });
    CC_CHECK(same.width == 0.0f && same.height == 0.0f);

    // --- SelectInRect: body centers are (96,96)=a and (288,160)=b ---
    DeselectAll(registry);
    CC_CHECK(SelectInRect(registry, { 80.0f, 80.0f, 240.0f, 120.0f }, false) == 2);
    CC_CHECK(registry.Get<Unit>(a)->isSelected && registry.Get<Unit>(b)->isSelected);
    CC_CHECK(SelectInRect(registry, { 80.0f, 80.0f, 40.0f, 40.0f }, false) == 1);
    CC_CHECK(registry.Get<Unit>(a)->isSelected);
    CC_CHECK(!registry.Get<Unit>(b)->isSelected);
    // Shift extends the selection instead of replacing it.
    CC_CHECK(SelectInRect(registry, { 270.0f, 140.0f, 60.0f, 60.0f }, true) == 1);
    CC_CHECK(registry.Get<Unit>(a)->isSelected && registry.Get<Unit>(b)->isSelected);
    // Empty box in replace mode clears everything.
    CC_CHECK(SelectInRect(registry, { 600.0f, 600.0f, 50.0f, 50.0f }, false) == 0);
    CC_CHECK(SelectedUnit(registry) == kInvalidEntity);

    // --- DraggedWorldBox: screen drag -> normalized world rect ---
    GameCamera camera;
    camera.view.offset = { 400.0f, 225.0f };
    camera.view.zoom = 1.0f;
    camera.view.target = { 64.0f, 64.0f };
    // Screen (400,225) maps to world (64,64); +64px right maps +64 world.
    const Rectangle dragged =
        DraggedWorldBox(camera, { 464.0f, 257.0f }, { 400.0f, 225.0f });
    CC_CHECK(CcNear(dragged.x, 64.0f) && CcNear(dragged.y, 64.0f));
    CC_CHECK(CcNear(dragged.width, 64.0f) && CcNear(dragged.height, 32.0f));
    // Matches the manual NormalizeRect(ScreenToWorld x2) it replaced.
    const Rectangle manual = NormalizeRect(camera.ScreenToWorld({ 464.0f, 257.0f }),
                                           camera.ScreenToWorld({ 400.0f, 225.0f }));
    CC_CHECK(dragged.x == manual.x && dragged.y == manual.y);
    CC_CHECK(dragged.width == manual.width && dragged.height == manual.height);

    // --- SelectAllOfType: same-team same-type only; add extends ---
    Registry typeReg;
    const Entity infA = SpawnAt(typeReg, 64.0f, 64.0f);
    typeReg.Get<Unit>(infA)->type = UnitType::RifleInfantry;
    const Entity infB = SpawnAt(typeReg, 256.0f, 64.0f);
    typeReg.Get<Unit>(infB)->type = UnitType::RifleInfantry;
    const Entity tank = SpawnAt(typeReg, 64.0f, 256.0f);
    typeReg.Get<Unit>(tank)->type = UnitType::LightTank;
    const Entity foeInf = SpawnAt(typeReg, 256.0f, 256.0f);
    typeReg.Get<Unit>(foeInf)->type = UnitType::RifleInfantry;
    typeReg.Get<Unit>(foeInf)->teamID = 1;
    CC_CHECK(SelectAllOfType(typeReg, UnitType::RifleInfantry, 0, false) == 2);
    CC_CHECK(typeReg.Get<Unit>(infA)->isSelected && typeReg.Get<Unit>(infB)->isSelected);
    CC_CHECK(!typeReg.Get<Unit>(tank)->isSelected && !typeReg.Get<Unit>(foeInf)->isSelected);
    CC_CHECK(SelectAllOfType(typeReg, UnitType::LightTank, 0, true) == 1); // extends
    CC_CHECK(typeReg.Get<Unit>(infA)->isSelected && typeReg.Get<Unit>(tank)->isSelected);

    // --- SelectAllOfTypeInRect: type + team + viewport, replace/extend ---
    Registry viewReg;
    const Entity inA = SpawnAt(viewReg, 64.0f, 64.0f); // center (96,96): inside
    viewReg.Get<Unit>(inA)->type = UnitType::RifleInfantry;
    const Entity inB = SpawnAt(viewReg, 192.0f, 64.0f); // center (224,96): inside
    viewReg.Get<Unit>(inB)->type = UnitType::RifleInfantry;
    const Entity viewTank = SpawnAt(viewReg, 64.0f, 192.0f); // inside, wrong type
    viewReg.Get<Unit>(viewTank)->type = UnitType::LightTank;
    const Entity viewFoe = SpawnAt(viewReg, 320.0f, 64.0f); // inside, enemy team
    viewReg.Get<Unit>(viewFoe)->type = UnitType::RifleInfantry;
    viewReg.Get<Unit>(viewFoe)->teamID = 1;
    const Entity offView = SpawnAt(viewReg, 2000.0f, 2000.0f); // same type/team, offscreen
    viewReg.Get<Unit>(offView)->type = UnitType::RifleInfantry;
    const Rectangle viewport = { 0.0f, 0.0f, 500.0f, 500.0f };

    CC_CHECK(SelectAllOfTypeInRect(viewReg, viewport, UnitType::RifleInfantry, 0, false) == 2);
    CC_CHECK(viewReg.Get<Unit>(inA)->isSelected && viewReg.Get<Unit>(inB)->isSelected);
    CC_CHECK(!viewReg.Get<Unit>(viewTank)->isSelected);
    CC_CHECK(!viewReg.Get<Unit>(viewFoe)->isSelected);
    CC_CHECK(!viewReg.Get<Unit>(offView)->isSelected);
    // add=true keeps the prior selection and still counts only matches.
    viewReg.Get<Unit>(viewTank)->isSelected = true;
    CC_CHECK(SelectAllOfTypeInRect(viewReg, viewport, UnitType::RifleInfantry, 0, true) == 2);
    CC_CHECK(viewReg.Get<Unit>(viewTank)->isSelected); // kept, not counted
    CC_CHECK(viewReg.Get<Unit>(inA)->isSelected);
}
