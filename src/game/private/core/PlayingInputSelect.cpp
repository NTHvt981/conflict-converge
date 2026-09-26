
#include "core/PlayingInput.h"

#include "units/Unit.h"
#include "app/input/Selection.h"
#include "raylib.h"

void PlayingInput::HandleLeftPress()
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
}

void PlayingInput::HandleSelectDragRelease()
{
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
}

void PlayingInput::HandleControlGroups()
{
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
}

void PlayingInput::HandlePlacementKeys()
{
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
