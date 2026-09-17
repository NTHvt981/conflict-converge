#include "Cursor.h"

#include "Selection.h" // PickUnitAt (pure, per-frame-safe hover query)
#include "Unit.h"      // teamID/type + CanRepairTarget

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
        if (hitUnit != nullptr && hitUnit->teamID != selected->teamID)
        {
            return CursorIntent::Attack;
        }
        if (selected->type == UnitType::Engineer && CanRepairTarget(registry, *selected, hit))
        {
            return CursorIntent::Repair;
        }
    }
    return CursorIntent::Move;
}
