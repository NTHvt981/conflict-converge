#pragma once

#include <optional>

#include "app/Audio.h"
#include "economy/Building.h"
#include "app/GameCamera.h"
#include "app/Hud.h"
#include "app/InputManager.h"
#include "app/Menu.h"
#include "app/Minimap.h"
#include "economy/Nodes.h"
#include "core/Registry.h"
#include "core/Subsystem.h"
#include "world/TileMap.h"

// In-match input dispatch. Runs only while Playing; advances no simulation itself.
class PlayingInput : public Subsystem
{
public:
    PlayingInput(Registry &registry, TileMap &map, OccupancyGrid &occ,
                 ResourceNodes &nodes, GameCamera &camera, Minimap &minimap,
                 InputManager &input, Audio &audio, const MenuSettings &settings,
                 Vector2 &rallyPos);
    PlayingInput(const PlayingInput &) = delete;
    PlayingInput &operator=(const PlayingInput &) = delete;

    void Dispatch();
    // Sticky modes persist.
    void ResetForMatch();
    // Shortcut toggles (guards live in Game's BindShortcuts).
    void ToggleSettingRally();
    void ToggleSlowestSpeed();
    void ToggleAreaBuild();
    void SelectPlacingType(BuildingType type);
    // HUD ability buttons arm targeted orders (attack-move, patrol, heal);
    // the next right-click issues them. Re-arming the same id disarms.
    void ArmAbility(AbilityId id);
    void ClearArmedAbility();
    const std::optional<AbilityId> &ArmedAbility() const;
    // Issues the armed ability at a world target; false when nothing acted
    // (ability stays armed). Headless-testable; Dispatch calls it on right-click.
    bool IssueArmedAbilityAt(Vector2 worldTarget, bool queued);
    void ToggleAreaRepair();
    void ToggleAttackGround();
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
    std::optional<BuildingType> placingType_;
    bool areaRepairMode_ = false;
    bool moveAtSlowestSpeed_ = false;
    int autoAddGroupBit_ = -1;
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
    std::optional<AbilityId> armedAbility_;
};
