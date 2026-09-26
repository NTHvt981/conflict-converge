
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
    if (placeDragActive_ && !input_.LeftDown())
    {
        placeDragActive_ = false;
        if (placingType_.has_value())
        {
            const Rectangle box =
                DraggedWorldBox(camera_, placeDragStart_, input_.MouseScreen());
            if (box.width < 6.0f && box.height < 6.0f)
            {
                const cc::IVec2 tile =
                    cc::WorldToTile(cc::ToGlm(input_.MouseWorld(camera_)));
                if (PlaceBuilding(registry_, map_, *placingType_, 0, tile.x, tile.y,
                                  &nodes_) != kInvalidEntity)
                {
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
                for (const cc::IVec2 &slot :
                     AreaBuildSlots(*placingType_, { std::min(a.x, b.x),
                                                    std::min(a.y, b.y) },
                                    { std::max(a.x, b.x), std::max(a.y, b.y) }))
                {
                    if (PlaceBuilding(registry_, map_, *placingType_, 0, slot.x, slot.y,
                                      &nodes_) != kInvalidEntity)
                    {
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
    if (repairDragActive_ && !input_.LeftDown())
    {
        repairDragActive_ = false;
        if (areaRepairMode_)
        {
            const Rectangle zone =
                DraggedWorldBox(camera_, repairDragStart_, input_.MouseScreen());
            std::vector<Entity> engineers;
            registry_.Each<Unit>([&](Entity id, const Unit &unit) {
                if (unit.isSelected &&
                    (unit.type == UnitType::Engineer || unit.type == UnitType::Medic))
                {
                    engineers.push_back(id);
                }
            });
            std::vector<Entity> candidates;
            CollectAreaRepairCandidates(registry_, zone, 0, candidates);
            std::vector<RepairAssignment> assignments;
            AssignAreaRepair(registry_, engineers, candidates, assignments);
            for (const RepairAssignment &job : assignments)
            {
                if (Unit *engineer = registry_.Get<Unit>(job.engineer))
                {
                    IssueOrEnqueue(*engineer, GetOrders(registry_, job.engineer),
                                   GetMover(registry_, job.engineer), map_, &occ_, job.engineer, registry_.Generation(job.engineer),
                                   input_.ShiftDown(),
                                   QueuedOrder{ QueuedOrderKind::Repair, {}, {},
                                                job.target });
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
        if (placingType_.has_value())
        {
            placingType_.reset();
            placeDragActive_ = false;
        }
        else if (areaRepairMode_)
        {
            areaRepairMode_ = false;
            repairDragActive_ = false;
        }
        else
        {
            const bool altDown = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
            if (altDown)
            {
                rightDragging_ = true;
                rightDragStart_ = input_.MouseScreen();
            }
        }
    }
}

void PlayingInput::DispatchRightClickOrders()
{
    if (armedAbility_.has_value())
    {
        if (IssueArmedAbilityAt(input_.MouseWorld(camera_), input_.ShiftDown()))
        {
            audio_.Play(SfxId::Confirm);
        }
    }
    else if (attackGroundMode_)
    {
        attackGroundMode_ = false;
        const Vector2 target = input_.MouseWorld(camera_);
        const bool queued = input_.ShiftDown();
        registry_.Each<Unit>([&](Entity id, Unit &unit) {
            if (unit.isSelected)
            {
                IssueOrEnqueue(unit, GetOrders(registry_, id), GetMover(registry_, id), map_,
                               &occ_, id,
                               registry_.Generation(id), queued,
                               QueuedOrder{ QueuedOrderKind::AttackGround, target });
            }
        });
        audio_.Play(SfxId::Confirm);
    }
    else
    {
    std::vector<Entity> squad;
    registry_.Each<Unit>([&](Entity id, const Unit &unit) {
        if (unit.isSelected)
        {
            squad.push_back(id);
        }
    });
    if (squad.size() == 1)
    {
        if (Unit *ordered = registry_.Get<Unit>(squad[0]))
        {
            Orders &orders = GetOrders(registry_, squad[0]);
            Mover &mover = GetMover(registry_, squad[0]);
            bool repaired = false;
            if (ordered->type == UnitType::Engineer)
            {
                const Vector2 world = input_.MouseWorld(camera_);
                Entity patient = PickUnitAt(registry_, world);
                if (patient == kInvalidEntity ||
                    !CanRepairTarget(registry_, *ordered, patient))
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
                            cc::TileToWorld(tile.x, tile.y) + cc::Vec2(32.0f, 32.0f));
                        if (CheckCollisionPointRec(tileCenter, footprint) &&
                            CanRepairTarget(registry_, *ordered, id))
                        {
                            patient = id;
                        }
                    });
                }
                if (patient != kInvalidEntity)
                {
                    IssueOrEnqueue(*ordered, orders, mover, map_, &occ_, squad[0],
                                   registry_.Generation(squad[0]), input_.ShiftDown(),
                                   QueuedOrder{ QueuedOrderKind::Repair, {}, {}, patient });
                    repaired = true;
                }
            }
            if (!repaired && ordered->type == UnitType::Medic)
            {
                const Vector2 world = input_.MouseWorld(camera_);
                Entity patient = PickUnitAt(registry_, world);
                if (patient == kInvalidEntity ||
                    !CanHealTarget(registry_, *ordered, patient))
                {
                    patient = kInvalidEntity;
                }
                if (patient != kInvalidEntity)
                {
                    IssueOrEnqueue(*ordered, orders, mover, map_, &occ_, squad[0],
                                   registry_.Generation(squad[0]), input_.ShiftDown(),
                                   QueuedOrder{ QueuedOrderKind::Repair, {}, {}, patient });
                    repaired = true;
                }
            }
            if (!repaired && registry_.Has<Cargo>(squad[0]))
            {
                const Vector2 world = input_.MouseWorld(camera_);
                const Entity picked = PickUnitAt(registry_, world);
                if (picked == squad[0])
                {
                    if (const Cargo *cargo = registry_.Get<Cargo>(squad[0]);
                        cargo != nullptr && !cargo->passengers.empty())
                    {
                        IssueOrEnqueue(*ordered, orders, mover, map_, &occ_, squad[0],
                                       registry_.Generation(squad[0]), input_.ShiftDown(),
                                       QueuedOrder{ QueuedOrderKind::Unload,
                                                    ordered->position });
                        repaired = true;
                    }
                }
                else if (picked != kInvalidEntity &&
                         CanLoadTarget(registry_, squad[0], *ordered, picked))
                {
                    IssueOrEnqueue(*ordered, orders, mover, map_, &occ_, squad[0],
                                   registry_.Generation(squad[0]), input_.ShiftDown(),
                                   QueuedOrder{ QueuedOrderKind::Load, {}, {}, picked });
                    repaired = true;
                }
            }
            if (!repaired)
            {
                if (input_.ShiftDown())
                {
                    IssueOrEnqueue(*ordered, orders, mover, map_, &occ_, squad[0],
                                   registry_.Generation(squad[0]), true,
                                   QueuedOrder{ QueuedOrderKind::Move,
                                                input_.MouseWorld(camera_) });
                }
                else
                {
                    orders.orderQueue.clear();
                    ClearOrders(*ordered, orders, mover);
                    IssuePathOrderFootprint(*ordered, orders, mover, map_, occ_,
                                            input_.MouseWorld(camera_), squad[0],
                                            registry_.Generation(squad[0]));
                }
            }
            audio_.Play(SfxId::Confirm);
        }
    }
    else if (!squad.empty())
    {
        if (input_.ShiftDown())
        {
            const Vector2 dest = input_.MouseWorld(camera_);
            for (const Entity id : squad)
            {
                if (Unit *unit = registry_.Get<Unit>(id))
                {
                    IssueOrEnqueue(*unit, GetOrders(registry_, id), GetMover(registry_, id),
                                   map_, &occ_, id,
                                   registry_.Generation(id), true,
                                   QueuedOrder{ QueuedOrderKind::Move, dest });
                }
            }
        }
        else
        {
            formation::IssueFormationMoveFP(registry_, squad, map_, occ_,
                                            input_.MouseWorld(camera_),
                                            moveAtSlowestSpeed_);
        }
        audio_.Play(SfxId::Confirm);
    }
    }
}

void PlayingInput::HandleRightRelease(bool cancelledPlacement)
{
    const bool altDown = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
    if (input_.RightPressed() && !rightDragging_ && !cancelledPlacement)
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
    if (rightDragging_ && !input_.RightDown())
    {
        rightDragging_ = false;
        std::vector<Entity> squad;
        registry_.Each<Unit>([&](Entity id, const Unit &unit) {
            if (unit.isSelected)
            {
                squad.push_back(id);
            }
        });
        if (!squad.empty())
        {
            const auto [lineStart, lineEnd] =
                DraggedWorldLine(camera_, rightDragStart_, input_.MouseScreen());
            formation::IssueLineFormationMoveFP(registry_, squad, map_, occ_, lineStart,
                                                lineEnd, moveAtSlowestSpeed_);
            audio_.Play(SfxId::Confirm);
        }
    }
}
