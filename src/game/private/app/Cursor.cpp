#include "app/Cursor.h"

#include "units/Extensions.h"
#include "app/Selection.h"
#include "units/Unit.h"
#include "units/UnitStats.h"

CursorIntent PredictCursorIntent(Registry &registry, const Unit *selected, Vector2 worldPos)
{
    if (selected == nullptr)
    {
        return CursorIntent::Default;
    }
    const Entity hit = PickUnitAt(registry, worldPos);
    if (hit != kInvalidEntity)
    {
        const Unit *hitUnit = registry.Get<Unit>(hit);
        // Capability by type (BaseStats), not live power: previews, editors
        // and partially-built units must predict the same intent.
        const bool canAttack = BaseStats(selected->type).attackPower > 0;
        if (hitUnit != nullptr && hitUnit->teamID != selected->teamID && canAttack)
        {
            return CursorIntent::Attack;
        }
        if (selected->type == UnitType::Engineer && CanRepairTarget(registry, *selected, hit))
        {
            return CursorIntent::Repair;
        }
        if (selected->type == UnitType::Medic && CanHealTarget(registry, *selected, hit))
        {
            return CursorIntent::Heal;
        }
        // G4 load: resolve the selected entity for the Cargo fullness check.
        Entity selectedId = kInvalidEntity;
        registry.Each<Unit>([&](Entity id, Unit &unit) {
            if (&unit == selected)
            {
                selectedId = id;
            }
        });
        if (selectedId != kInvalidEntity && registry.Has<Cargo>(selectedId) &&
            CanLoadTarget(registry, selectedId, *selected, hit))
        {
            return CursorIntent::Load;
        }
    }
    return CursorIntent::Move;
}
