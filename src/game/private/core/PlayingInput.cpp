
#include "core/PlayingInput.h"

#include "app/input/Selection.h"
#include "units/UnitCommands.h"

PlayingInput::PlayingInput(Registry &registry, TileMap &map, OccupancyGrid &occ,
						   ResourceNodes &nodes, GameCamera &camera, Minimap &minimap,
						   InputManager &input, Audio &audio, const MenuSettings &settings,
						   ResourceSystem &resources)
	: registry_(registry)
	, map_(map)
	, occ_(occ)
	, nodes_(nodes)
	, camera_(camera)
	, minimap_(minimap)
	, input_(input)
	, audio_(audio)
	, settings_(settings)
	, resources_(resources)
{
}

void PlayingInput::SetMode(InteractionMode mode, bool enabled)
{
	if (enabled)
	{
		mode_ = mode_ | mode;
	}
	else
	{
		mode_ =
			static_cast<InteractionMode>(static_cast<int>(mode_) & ~static_cast<int>(mode));
	}
}

void PlayingInput::ResetForMatch()
{
	SetMode(InteractionMode::SettingRally, false);
	selectDrag_.active = false;
	rightDrag_.active = false;
	pendingRightClick_ = false;
	rightDragDist_ = 0.0f;
	placeDrag_.active = false;
	repairDrag_.active = false;
	SetMode(InteractionMode::AttackGround, false);
	armedAbility_.reset();
}

void PlayingInput::ToggleSettingRally()
{
	SetMode(InteractionMode::SettingRally, !HasMode(mode_, InteractionMode::SettingRally));
}

void PlayingInput::ToggleSlowestSpeed()
{
	SetMode(InteractionMode::SlowestSpeed, !HasMode(mode_, InteractionMode::SlowestSpeed));
}

void PlayingInput::ToggleAreaBuild()
{
	if (HasMode(mode_, InteractionMode::Placing))
	{
		SetMode(InteractionMode::Placing, false);
		placingType_.reset();
	}
	else
	{
		SetMode(InteractionMode::Placing, true);
		placingType_ = BuildingType::Base;
	}
	placeDrag_.active = false;
}

void PlayingInput::SelectPlacingType(BuildingType type)
{
	SetMode(InteractionMode::Placing, true);
	placingType_ = type;
	placeDrag_.active = false;
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
	SetMode(InteractionMode::AreaRepair, !HasMode(mode_, InteractionMode::AreaRepair));
	repairDrag_.active = false;
}

void PlayingInput::ToggleAttackGround()
{
	SetMode(InteractionMode::AttackGround, !HasMode(mode_, InteractionMode::AttackGround));
}

bool PlayingInput::IsDragging() const
{
	return selectDrag_.active;
}

Vector2 PlayingInput::DragStart() const
{
	return selectDrag_.start;
}

bool PlayingInput::IsRightDragging() const
{
	return rightDrag_.active;
}

Vector2 PlayingInput::RightDragStart() const
{
	return rightDrag_.start;
}

bool PlayingInput::RepairDragActive() const
{
	return repairDrag_.active;
}

Vector2 PlayingInput::RepairDragStart() const
{
	return repairDrag_.start;
}

bool PlayingInput::PlaceDragActive() const
{
	return placeDrag_.active;
}

const std::optional<BuildingType> &PlayingInput::PlacingType() const
{
	return placingType_;
}

bool PlayingInput::AreaRepairMode() const
{
	return HasMode(mode_, InteractionMode::AreaRepair);
}

bool PlayingInput::AttackGroundMode() const
{
	return HasMode(mode_, InteractionMode::AttackGround);
}

bool PlayingInput::IsSettingRally() const
{
	return HasMode(mode_, InteractionMode::SettingRally);
}

int PlayingInput::AutoAddGroupBit() const
{
	return autoAddGroupBit_;
}

void PlayingInput::Dispatch(bool pointerOverUi)
{
	if (!pointerOverUi)
	{
		HandleLeftPress();
		HandleSelectDragRelease();
		HandlePlaceDragRelease();
		HandleRepairDragRelease();
		HandleRightPan();
		const bool wasPlacing = HasMode(mode_, InteractionMode::Placing) ||
								HasMode(mode_, InteractionMode::AreaRepair);
		HandleRightPress();
		const bool cancelledPlacement = input_.RightPressed() && wasPlacing;
		HandleRightRelease(cancelledPlacement);
		HandleLineDragRelease();
	}
	HandleControlGroups();
	HandlePlacementKeys();
}
