
#include "core/PlayingInput.h"

#include "app/input/Selection.h"
#include "units/UnitCommands.h"

PlayingInput::PlayingInput(Registry &registry, TileMap &map, OccupancyGrid &occ,
                           ResourceNodes &nodes, GameCamera &camera, Minimap &minimap,
                           InputManager &input, Audio &audio, const MenuSettings &settings,
                           Vector2 &rallyPos)
    : registry_(registry)
    , map_(map)
    , occ_(occ)
    , nodes_(nodes)
    , camera_(camera)
    , minimap_(minimap)
    , input_(input)
    , audio_(audio)
    , settings_(settings)
    , rallyPos_(rallyPos)
{
}

void PlayingInput::ResetForMatch()
{
    settingRally_ = false;
    dragging_ = false;
    rightDragging_ = false;
    pendingRightClick_ = false;
    rightDragDist_ = 0.0f;
    placeDragActive_ = false;
    repairDragActive_ = false;
    attackGroundMode_ = false;
    armedAbility_.reset();
}

void PlayingInput::ToggleSettingRally()
{
    settingRally_ = !settingRally_;
}

void PlayingInput::ToggleSlowestSpeed()
{
    moveAtSlowestSpeed_ = !moveAtSlowestSpeed_;
}

void PlayingInput::ToggleAreaBuild()
{
    if (placingType_.has_value())
    {
        placingType_.reset();
    }
    else
    {
        placingType_ = BuildingType::Base;
    }
    placeDragActive_ = false;
}

void PlayingInput::SelectPlacingType(BuildingType type)
{
    placingType_ = type;
    placeDragActive_ = false;
}

void PlayingInput::ArmAbility(AbilityId id)
{
    if (armedAbility_.has_value() && *armedAbility_ == id)
    {
        armedAbility_.reset();
    }
    else
    {
        armedAbility_ = id;
    }
}

void PlayingInput::ClearArmedAbility()
{
    armedAbility_.reset();
}

const std::optional<AbilityId> &PlayingInput::ArmedAbility() const
{
    return armedAbility_;
}

bool PlayingInput::IssueArmedAbilityAt(Vector2 worldTarget, bool queued)
{
    if (!armedAbility_.has_value())
    {
        return false;
    }
    const AbilityId armed = *armedAbility_;
    int acted = 0;
    if (armed == AbilityId::AttackMove)
    {
        acted = IssueSelectionAttackMove(registry_, map_, &occ_, worldTarget, queued);
    }
    else if (armed == AbilityId::Patrol)
    {
        acted = IssueSelectionPatrol(registry_, map_, &occ_, worldTarget, queued);
    }
    else if (armed == AbilityId::Heal)
    {
        const Entity patient = PickUnitAt(registry_, worldTarget);
        if (patient != kInvalidEntity)
        {
            acted = IssueSelectionHeal(registry_, map_, &occ_, patient, queued);
        }
    }
    if (acted > 0)
    {
        armedAbility_.reset();
        return true;
    }
    return false;
}

void PlayingInput::ToggleAreaRepair()
{
    areaRepairMode_ = !areaRepairMode_;
    repairDragActive_ = false;
}

void PlayingInput::ToggleAttackGround()
{
    attackGroundMode_ = !attackGroundMode_;
}

bool PlayingInput::IsDragging() const
{
    return dragging_;
}

Vector2 PlayingInput::DragStart() const
{
    return dragStart_;
}

bool PlayingInput::IsRightDragging() const
{
    return rightDragging_;
}

Vector2 PlayingInput::RightDragStart() const
{
    return rightDragStart_;
}

bool PlayingInput::RepairDragActive() const
{
    return repairDragActive_;
}

Vector2 PlayingInput::RepairDragStart() const
{
    return repairDragStart_;
}

bool PlayingInput::PlaceDragActive() const
{
    return placeDragActive_;
}

const std::optional<BuildingType> &PlayingInput::PlacingType() const
{
    return placingType_;
}

bool PlayingInput::AreaRepairMode() const
{
    return areaRepairMode_;
}

bool PlayingInput::AttackGroundMode() const
{
    return attackGroundMode_;
}

bool PlayingInput::IsSettingRally() const
{
    return settingRally_;
}

const int &PlayingInput::AutoAddGroupBit() const
{
    return autoAddGroupBit_;
}

void PlayingInput::Dispatch()
{
    HandleLeftPress();
    HandleSelectDragRelease();
    HandlePlaceDragRelease();
    HandleRepairDragRelease();
    HandleRightPan();
    const bool wasPlacing = placingType_.has_value() || areaRepairMode_;
    HandleRightPress();
    const bool cancelledPlacement = input_.RightPressed() && wasPlacing;
    HandleRightRelease(cancelledPlacement);
    HandleLineDragRelease();
    HandleControlGroups();
    HandlePlacementKeys();
}
