
#include "core/PlayingInput.h"

#include "units/Extensions.h"
#include "units/Formation.h"
#include "world/Pathfinder.h"
#include "app/input/Selection.h"
#include "units/Unit.h"
#include "units/UnitCommands.h"
#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <vector>

void PlayingInput::HandlePlaceDragRelease()
{
	if (placeDrag_.active && !input_.LeftDown())
	{
		placeDrag_.active = false;
		if (placingType_.has_value())
		{
			const Rectangle box =
				DraggedWorldBox(camera_, placeDrag_.start, input_.MouseScreen());
			if (box.width < 6.0f && box.height < 6.0f)
			{
				const cc::IVec2 tile =
					cc::WorldToTile(cc::ToGlm(input_.MouseWorld(camera_)));
				const UnitCost cost = BuildingCost(*placingType_);
				if (resources_.iron >= cost.iron && resources_.oil >= cost.oil &&
					PlaceBuilding(registry_, map_, *placingType_, kPlayerTeam, tile.x, tile.y,
								  &nodes_) != kInvalidEntity)
				{
					resources_.TrySpend(cost.iron, cost.oil);
					audio_.Play(SfxId::Place);
				}
			}
			else
			{
				const cc::IVec2 a =
					cc::WorldToTile(cc::ToGlm({ box.x, box.y }));
				const cc::IVec2 b = cc::WorldToTile(
					cc::ToGlm({ box.x + box.width, box.y + box.height }));
				int placed = 0;
				const UnitCost areaCost = BuildingCost(*placingType_);
				for (const cc::IVec2 &slot :
					 AreaBuildSlots(*placingType_, { std::min(a.x, b.x),
													std::min(a.y, b.y) },
									{ std::max(a.x, b.x), std::max(a.y, b.y) }))
				{
					if (resources_.iron < areaCost.iron || resources_.oil < areaCost.oil)
					{
						break;
					}
					if (PlaceBuilding(registry_, map_, *placingType_, kPlayerTeam, slot.x, slot.y,
									  &nodes_) != kInvalidEntity)
					{
						resources_.TrySpend(areaCost.iron, areaCost.oil);
						++placed;
					}
				}
				if (placed > 0)
				{
					audio_.Play(SfxId::Place);
				}
			}
		}
	}
}

void PlayingInput::HandleRepairDragRelease()
{
	if (repairDrag_.active && !input_.LeftDown())
	{
		repairDrag_.active = false;
		if (HasMode(mode_, InteractionMode::AreaRepair))
		{
			const Rectangle zone =
				DraggedWorldBox(camera_, repairDrag_.start, input_.MouseScreen());
			std::vector<Entity> engineers;
			registry_.Each<Unit>([&](Entity id, const Unit &unit) {
				if (unit.isSelected &&
					(unit.type == UnitType::Engineer || unit.type == UnitType::Medic))
				{
					engineers.push_back(id);
				}
			});
			std::vector<Entity> candidates;
			CollectAreaRepairCandidates(registry_, zone, kPlayerTeam, candidates);
			std::vector<RepairAssignment> assignments;
			AssignAreaRepair(registry_, engineers, candidates, assignments);
			for (const RepairAssignment &job : assignments)
			{
				if (Unit *engineer = registry_.Get<Unit>(job.engineer))
				{
					IssueOrEnqueue(*engineer,
								   MakeOrderContext(registry_, job.engineer, map_, &occ_),
								   input_.ShiftDown(),
								   QueuedOrder{ .kind = QueuedOrderKind::Repair, .target = job.target });
				}
			}
			if (!assignments.empty())
			{
				audio_.Play(SfxId::Confirm);
			}
		}
	}
}

void PlayingInput::HandleRightPan()
{
	const bool altDown = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
	if (input_.RightDown() && !altDown)
	{
		const Vector2 panDelta = input_.MouseDeltaScreen();
		rightDragDist_ += std::sqrt(panDelta.x * panDelta.x + panDelta.y * panDelta.y);
		if (settings_.rightDragPan && rightDragDist_ > 6.0f)
		{
			const float zoom = camera_.view.zoom <= 0.0f ? 1.0f : camera_.view.zoom;
			camera_.Pan({ -panDelta.x / zoom, -panDelta.y / zoom });
		}
	}
}

void PlayingInput::HandleRightPress()
{
	if (input_.RightPressed())
	{
		if (HasMode(mode_, InteractionMode::Placing))
		{
			SetMode(InteractionMode::Placing, false);
			placingType_.reset();
			placeDrag_.active = false;
		}
		else if (HasMode(mode_, InteractionMode::AreaRepair))
		{
			SetMode(InteractionMode::AreaRepair, false);
			repairDrag_.active = false;
		}
		else
		{
			const bool altDown = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
			if (altDown)
			{
				rightDrag_.active = true;
				rightDrag_.start = input_.MouseScreen();
			}
		}
	}
}

void PlayingInput::DispatchRightClickOrders()
{
	const Vector2 world = input_.MouseWorld(camera_);
	if (DispatchArmedAbility(world))
	{
		return;
	}
	if (DispatchAttackGround(world))
	{
		return;
	}
	const std::vector<Entity> squad = SelectedUnits();
	if (squad.empty())
	{
		return;
	}
	if (squad.size() == 1)
	{
		DispatchSingleUnitOrder(squad[0], world);
	}
	else
	{
		DispatchSquadOrder(squad, world);
	}
}

std::vector<Entity> PlayingInput::SelectedUnits() const
{
	std::vector<Entity> squad;
	registry_.Each<Unit>([&](Entity id, const Unit &unit) {
		if (unit.isSelected)
		{
			squad.push_back(id);
		}
	});
	return squad;
}

bool PlayingInput::DispatchArmedAbility(Vector2 world)
{
	if (!armedAbility_.has_value())
	{
		return false;
	}
	if (IssueArmedAbilityAt(world, input_.ShiftDown()))
	{
		audio_.Play(SfxId::Confirm);
	}
	return true;
}

bool PlayingInput::DispatchAttackGround(Vector2 world)
{
	if (!HasMode(mode_, InteractionMode::AttackGround))
	{
		return false;
	}
	SetMode(InteractionMode::AttackGround, false);
	const bool queued = input_.ShiftDown();
	registry_.Each<Unit>([&](Entity id, Unit &unit) {
		if (unit.isSelected)
		{
			IssueOrEnqueue(unit, MakeOrderContext(registry_, id, map_, &occ_), queued,
						   QueuedOrder{ .kind = QueuedOrderKind::AttackGround, .pointA = world });
		}
	});
	audio_.Play(SfxId::Confirm);
	return true;
}

void PlayingInput::DispatchSingleUnitOrder(Entity id, Vector2 world)
{
	Unit *ordered = registry_.Get<Unit>(id);
	if (ordered == nullptr)
	{
		return;
	}
	Orders &orders = GetOrders(registry_, id);
	Mover &mover = GetMover(registry_, id);
	const OrderContext ctx = MakeOrderContext(registry_, id, map_, &occ_);
	const bool queued = input_.ShiftDown();
	if (TryIssueSpecialOrder(*ordered, id, ctx, world, queued))
	{
		audio_.Play(SfxId::Confirm);
		return;
	}
	if (queued)
	{
		IssueOrEnqueue(*ordered, ctx, true,
					   QueuedOrder{ .kind = QueuedOrderKind::Move, .pointA = world });
	}
	else
	{
		orders.orderQueue.clear();
		ClearOrders(*ordered, orders, mover);
		IssuePathOrderFootprint(*ordered, ctx, world);
	}
	audio_.Play(SfxId::Confirm);
}

void PlayingInput::DispatchSquadOrder(const std::vector<Entity> &squad, Vector2 world)
{
	if (input_.ShiftDown())
	{
		for (const Entity id : squad)
		{
			if (Unit *unit = registry_.Get<Unit>(id))
			{
				IssueOrEnqueue(*unit, MakeOrderContext(registry_, id, map_, &occ_), true,
							   QueuedOrder{ .kind = QueuedOrderKind::Move, .pointA = world });
			}
		}
	}
	else
	{
		formation::IssueFormationMoveFP(registry_, squad, map_, occ_, world,
										HasMode(mode_, InteractionMode::SlowestSpeed));
	}
	audio_.Play(SfxId::Confirm);
}

bool PlayingInput::TryIssueSpecialOrder(Unit &unit, Entity id, const OrderContext &ctx,
										Vector2 world, bool queued)
{
	if (unit.type == UnitType::Engineer)
	{
		Entity patient = PickUnitAt(registry_, world);
		if (patient == kInvalidEntity ||
			!CanRepairTarget(registry_, unit, patient))
		{
			patient = kInvalidEntity;
			const cc::IVec2 tile = cc::WorldToTile(cc::ToGlm(world));
			registry_.Each<Building>([&](Entity id, const Building &building) {
				if (patient != kInvalidEntity)
				{
					return;
				}
				const Rectangle footprint = BuildingFootprintRect(building);
				const Vector2 tileCenter = cc::ToRaylib(
					cc::TileToWorld(tile.x, tile.y) +
						cc::Vec2(cc::kHalfTile, cc::kHalfTile));
				if (CheckCollisionPointRec(tileCenter, footprint) &&
					CanRepairTarget(registry_, unit, id))
				{
					patient = id;
				}
			});
		}
		if (patient != kInvalidEntity)
		{
			IssueOrEnqueue(unit, ctx, queued,
						   QueuedOrder{ .kind = QueuedOrderKind::Repair, .target = patient });
			return true;
		}
	}
	if (unit.type == UnitType::Medic)
	{
		Entity patient = PickUnitAt(registry_, world);
		if (patient == kInvalidEntity ||
			!CanHealTarget(registry_, unit, patient))
		{
			patient = kInvalidEntity;
		}
		if (patient != kInvalidEntity)
		{
			IssueOrEnqueue(unit, ctx, queued,
						   QueuedOrder{ .kind = QueuedOrderKind::Repair, .target = patient });
			return true;
		}
	}
	if (registry_.Has<Cargo>(id))
	{
		const Entity picked = PickUnitAt(registry_, world);
		if (picked == id)
		{
			if (const Cargo *cargo = registry_.Get<Cargo>(id);
				cargo != nullptr && !cargo->passengers.empty())
			{
				IssueOrEnqueue(unit, ctx, queued,
							   QueuedOrder{ .kind = QueuedOrderKind::Unload, .pointA = unit.position });
				return true;
			}
		}
		else if (picked != kInvalidEntity &&
				 CanLoadTarget(registry_, id, unit, picked))
		{
			IssueOrEnqueue(unit, ctx, queued,
						   QueuedOrder{ .kind = QueuedOrderKind::Load, .target = picked });
			return true;
		}
	}
	return false;
}

void PlayingInput::HandleRightRelease(bool cancelledPlacement)
{
	const bool altDown = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
	if (input_.RightPressed() && !rightDrag_.active && !cancelledPlacement)
	{
		if (settings_.rightDragPan && !altDown)
		{
			pendingRightClick_ = true;
		}
		else
		{
			DispatchRightClickOrders();
		}
	}
	if (!input_.RightDown())
	{
		if (pendingRightClick_ && rightDragDist_ < 6.0f)
		{
			DispatchRightClickOrders();
		}
		pendingRightClick_ = false;
		rightDragDist_ = 0.0f;
	}
}

void PlayingInput::HandleLineDragRelease()
{
	if (rightDrag_.active && !input_.RightDown())
	{
		rightDrag_.active = false;
		const std::vector<Entity> squad = SelectedUnits();
		if (!squad.empty())
		{
			const auto [lineStart, lineEnd] =
				DraggedWorldLine(camera_, rightDrag_.start, input_.MouseScreen());
			formation::IssueLineFormationMoveFP(registry_, squad, map_, occ_, lineStart,
												lineEnd,
												HasMode(mode_, InteractionMode::SlowestSpeed));
			audio_.Play(SfxId::Confirm);
		}
	}
}
