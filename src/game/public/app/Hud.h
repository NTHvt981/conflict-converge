#pragma once

#include <string>
#include <vector>

#include "Registry.h"
#include "ResourceSystem.h"
#include "Unit.h"
#include "Production.h"

class Art;
enum class BuildingType;
class HotkeyMap;

// HUD panels. Text content is built by pure functions (tested); the Draw*
// wrappers are thin raygui calls owned by main.

const char *UnitTypeName(UnitType type);
const char *BuildingTypeName(BuildingType type);
std::string FormatResources(const ResourceSystem &resources);
std::string SelectionSummary(const Unit &unit);
// Selection visuals + shortcut overlay builders.
float UnitHealthFraction(const Unit &unit); // hp / max, clamped to [0, 1]
std::vector<std::string> ShortcutHintLines(const HotkeyMap &hotkeys);
// Hover tooltip (world-space unit info): debounce state + pure updater.
struct HoverTooltipState
{
    Entity hovered = kInvalidEntity;
    float time = 0.0f;
};
constexpr float kHoverTooltipDelay = 0.4f;
bool UpdateHoverTooltip(HoverTooltipState &state, Entity hovered, float dt, float delay);
// Two-line stat block for a hovered unit (summary + stance/state).
std::vector<std::string> UnitTooltipLines(const Unit &unit);

// Screen-space panels (call after EndMode2D). Optional art draws 16px icons;
// nullptr keeps text-only.
void DrawResourcePanel(const ResourceSystem &resources, const Art *art = nullptr);
// Factory production UI. Display order for the 7 buildable types.
std::vector<UnitType> ProductionMenuOrder();
// Factory panel with per-type cost buttons, queue count + progress, and a
// cancel-top button. hasFactory=false shows a "Need Factory" stub. Returns
// the clicked type, or kNoProductionClick.
constexpr int kNoProductionClick = -1;
int DrawProductionPanel(ResourceSystem &resources, ProductionQueue &queue, bool hasFactory);
// Named-slot readout (filled/empty via file existence).
void DrawSaveSlots();
void DrawSelectionPanel(Registry &registry, const Art *art = nullptr);
// QoL auto-repair panel: global checkbox + rate-cap slider.
void DrawRepairPanel(bool *enabled, float *capFraction);
// QoL idle-select buttons with live counts (greyed at 0).
void DrawIdleButtons(Registry &registry, int teamID);
// QoL control-group strip (10 boxes: member counts, selection + auto-add highlight).
void DrawControlGroupStrip(Registry &registry, int teamID, int autoAddGroupBit,
                           const Art *art = nullptr);
