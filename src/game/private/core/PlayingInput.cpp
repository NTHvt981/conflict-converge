
#include "PlayingInput.h"

#include "Extensions.h"
#include "Formation.h"
#include "Pathfinder.h"
#include "Selection.h"
#include "Unit.h"
#include <algorithm>
#include <cmath>
#include <vector>

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
    if (input_.LeftPressed())
    {
        if (placingType_.has_value())
        {
            placeDragActive_ = true;
            placeDragStart_ = input_.MouseScreen();
        }
        else if (areaRepairMode_)
        {
            repairDragActive_ = true;
            repairDragStart_ = input_.MouseScreen();
        }
        else if (minimap_.Contains(input_.MouseScreen()))
        {
            camera_.view.target =
                minimap_.MinimapToWorld(input_.MouseScreen(), map_.Width(), map_.Height());
        }
        else if (settingRally_)
        {
            rallyPos_ = input_.MouseWorld(camera_);
            settingRally_ = false;
        }
        else
        {
            dragging_ = true;
            dragStart_ = input_.MouseScreen();
        }
    }
    if (dragging_ && !input_.LeftDown())
    {
        dragging_ = false;
        const Rectangle box = NormalizeRect(dragStart_, input_.MouseScreen());
        if (box.width < 6.0f && box.height < 6.0f)
        {
            const Vector2 world = input_.MouseWorld(camera_);
            const Entity hit = PickUnitAt(registry_, world);
            if (hit != kInvalidEntity)
            {
                const Unit *hitUnit = registry_.Get<Unit>(hit);
                if (hitUnit != nullptr && input_.DoubleClicked())
                {
                    SelectAllOfTypeInRect(
                        registry_,
                        DraggedWorldBox(camera_, { 0.0f, 0.0f },
                                        { static_cast<float>(GetScreenWidth()),
                                          static_cast<float>(GetScreenHeight()) }),
                        hitUnit->type, 0, false);
                }
                else
                {
                    SelectOnly(registry_, hit);
                }
                audio_.Play(SfxId::Select);
            }
            else
            {
                DeselectAll(registry_);
            }
        }
        else
        {
            if (SelectInRect(registry_,
                             DraggedWorldBox(camera_, { box.x, box.y },
                                             { box.x + box.width, box.y + box.height }),
                             input_.ShiftDown()) > 0)
            {
                audio_.Play(SfxId::Select);
            }
        }
    }
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
    const bool wasPlacing = placingType_.has_value() || areaRepairMode_;
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
        else if (altDown)
        {
            rightDragging_ = true;
            rightDragStart_ = input_.MouseScreen();
        }
    }
    const bool cancelledPlacement = input_.RightPressed() && wasPlacing;
    auto dispatchRightClickOrders = [&]() {
        if (attackGroundMode_)
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
    };
    if (input_.RightPressed() && !rightDragging_ && !cancelledPlacement)
    {
        if (settings_.rightDragPan && !altDown)
        {
            pendingRightClick_ = true;
        }
        else
        {
            dispatchRightClickOrders();
        }
    }
    if (!input_.RightDown())
    {
        if (pendingRightClick_ && rightDragDist_ < 6.0f)
        {
            dispatchRightClickOrders();
        }
        pendingRightClick_ = false;
        rightDragDist_ = 0.0f;
    }
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
    static constexpr int kGroupKeys[10] = { KEY_ONE,   KEY_TWO,   KEY_THREE, KEY_FOUR,
                                            KEY_FIVE,  KEY_SIX,   KEY_SEVEN, KEY_EIGHT,
                                            KEY_NINE,  KEY_ZERO };
    for (int bit = 0; bit < 10; ++bit)
    {
        if (!IsKeyPressed(kGroupKeys[bit]) || placingType_.has_value())
        {
            continue;
        }
        if (input_.CtrlDown() && input_.ShiftDown())
        {
            autoAddGroupBit_ = bit;
        }
        else if (input_.CtrlDown())
        {
            AssignControlGroup(registry_, bit);
        }
        else if (input_.ShiftDown())
        {
            AddToControlGroup(registry_, bit);
        }
        else
        {
            RecallControlGroup(registry_, bit);
        }
    }
    if (placingType_.has_value())
    {
        if (IsKeyPressed(KEY_ONE))
        {
            placingType_ = BuildingType::Base;
        }
        else if (IsKeyPressed(KEY_TWO))
        {
            placingType_ = BuildingType::ResourceDepot;
        }
        else if (IsKeyPressed(KEY_THREE))
        {
            placingType_ = BuildingType::Factory;
        }
    }
}
