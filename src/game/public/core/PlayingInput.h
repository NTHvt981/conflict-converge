#pragma once

#include <optional>
#include <vector>

#include "app/data/Audio.h"
#include "economy/Building.h"
#include "app/ui/GameCamera.h"
#include "app/ui/Hud.h"
#include "app/input/InputManager.h"
#include "app/match/Menu.h"
#include "app/ui/Minimap.h"
#include "economy/Nodes.h"
#include "core/Registry.h"
#include "core/Subsystem.h"
#include "world/TileMap.h"

struct Unit;
struct OrderContext;

enum class InteractionMode
{
	None = 0,
	Placing = 1 << 0,
	AreaRepair = 1 << 1,
	SettingRally = 1 << 2,
	AttackGround = 1 << 3,
	SlowestSpeed = 1 << 4
};

inline constexpr InteractionMode operator|(InteractionMode lhs, InteractionMode rhs)
{
	return static_cast<InteractionMode>(static_cast<int>(lhs) | static_cast<int>(rhs));
}

inline constexpr InteractionMode operator&(InteractionMode lhs, InteractionMode rhs)
{
	return static_cast<InteractionMode>(static_cast<int>(lhs) & static_cast<int>(rhs));
}

inline constexpr bool HasMode(InteractionMode set, InteractionMode mode)
{
	return (set & mode) != InteractionMode::None;
}

struct DragGesture
{
	bool active = false;
	Vector2 start = {};
};

// In-match input dispatch. Runs only while Playing; advances no simulation itself.
class PlayingInput : public Subsystem
{
public:
	PlayingInput(Registry &registry, TileMap &map, OccupancyGrid &occ,
				 ResourceNodes &nodes, GameCamera &camera, Minimap &minimap,
				 InputManager &input, Audio &audio, const MenuSettings &settings,
				 ResourceSystem &resources);
	PlayingInput(const PlayingInput &) = delete;
	PlayingInput &operator=(const PlayingInput &) = delete;

	void Dispatch(bool pointerOverUi);
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
	ResourceSystem &resources_;
	std::optional<BuildingType> placingType_;
	InteractionMode mode_ = InteractionMode::None;
	int autoAddGroupBit_ = -1;
	DragGesture selectDrag_;
	DragGesture rightDrag_;
	bool pendingRightClick_ = false;
	float rightDragDist_ = 0.0f;
	DragGesture placeDrag_;
	DragGesture repairDrag_;
	std::optional<AbilityId> armedAbility_;

	void SetMode(InteractionMode mode, bool enabled);
	void HandleLeftPress();
	void HandleSelectDragRelease();
	void HandleControlGroups();
	void HandlePlacementKeys();
	void HandlePlaceDragRelease();
	void HandleRepairDragRelease();
	void HandleRightPan();
	void HandleRightPress();
	void HandleRightRelease(bool cancelledPlacement);
	void DispatchRightClickOrders();
	std::vector<Entity> SelectedUnits() const;
	bool DispatchArmedAbility(Vector2 world);
	bool DispatchAttackGround(Vector2 world);
	void DispatchSingleUnitOrder(Entity id, Vector2 world);
	void DispatchSquadOrder(const std::vector<Entity> &squad, Vector2 world);
	bool TryIssueSpecialOrder(Unit &unit, Entity id, const OrderContext &ctx, Vector2 world,
							  bool queued);
	void HandleLineDragRelease();
};
