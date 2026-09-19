#pragma once

#include <optional>

#include "Audio.h"
#include "Building.h"
#include "GameCamera.h"
#include "InputManager.h"
#include "Menu.h"
#include "Minimap.h"
#include "Nodes.h"
#include "Registry.h"
#include "TileMap.h"

// PlayingInput owns the in-match input dispatch extracted from Game: drag-box
// select, area-build/repair gestures, right-click orders, line formation,
// control groups, and the sticky gesture modes. Runs only while Playing;
// advances no simulation itself.
class PlayingInput
{
public:
    PlayingInput(Registry &registry, TileMap &map, OccupancyGrid &occ,
                 ResourceNodes &nodes, GameCamera &camera, Minimap &minimap,
                 InputManager &input, Audio &audio, const MenuSettings &settings,
                 Vector2 &rallyPos);
    PlayingInput(const PlayingInput &) = delete;
    PlayingInput &operator=(const PlayingInput &) = delete;

    // Exactly one frame of gesture dispatch.
    void Dispatch();
    // Clear transient gesture state (sticky modes persist).
    void ResetForMatch();
    // Esc in Playing: stand down every gesture + mode, drop queued clicks, deselect.
    void CancelForEsc();
    // Shortcut toggles (guards live in Game's BindShortcuts).
    void ToggleSettingRally();
    void ToggleSlowestSpeed();
    void ToggleAreaBuild();
    void ToggleAreaRepair();
    void ToggleAttackGround();
    // Render/HUD reads (drag visuals, ghosts, previews, hints, tooltips).
    bool IsDragging() const;
    Vector2 DragStart() const;
    bool IsRightDragging() const;
    Vector2 RightDragStart() const;
    bool RepairDragActive() const;
    Vector2 RepairDragStart() const;
    bool PlaceDragActive() const;
    const std::optional<BuildingType> &PlacingType() const;
    bool AreaRepairMode() const;
    bool AttackGroundMode() const;
    bool IsSettingRally() const;
    const int &AutoAddGroupBit() const;

private:
    Registry &registry_;
    TileMap &map_;
    OccupancyGrid &occ_;
    ResourceNodes &nodes_;
    GameCamera &camera_;
    Minimap &minimap_;
    InputManager &input_;
    Audio &audio_;
    const MenuSettings &settings_;
    // Factory rally point (owned by Game: sim + load path share it).
    Vector2 &rallyPos_;
    // Sticky modes (persist across matches).
    std::optional<BuildingType> placingType_;
    bool areaRepairMode_ = false;
    bool moveAtSlowestSpeed_ = false;
    int autoAddGroupBit_ = -1;
    // Transient gesture state (ResetForMatch clears it).
    bool settingRally_ = false;
    bool dragging_ = false;
    Vector2 dragStart_ = {};
    bool rightDragging_ = false;
    Vector2 rightDragStart_ = {};
    bool pendingRightClick_ = false;
    float rightDragDist_ = 0.0f;
    bool placeDragActive_ = false;
    Vector2 placeDragStart_ = {};
    bool repairDragActive_ = false;
    Vector2 repairDragStart_ = {};
    bool attackGroundMode_ = false;
};
