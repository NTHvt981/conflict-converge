#pragma once

#include <string>
#include <vector>

#include "Registry.h"
#include "ResourceSystem.h"
#include "Unit.h"
#include "Production.h"

class Art; // fwd-decl (Hud.cpp includes Art.h for icons)
enum class BuildingType; // fwd-decl (Hud.cpp includes Building.h)
class HotkeyMap; // fwd-decl (Hud.cpp includes Hotkeys.h for live bindings)

// HUD panels. Text content is built by pure functions (tested);
// the Draw* wrappers below are thin raygui calls owned by main.cpp.

const char *UnitTypeName(UnitType type);
const char *BuildingTypeName(BuildingType type);
std::string FormatResources(const ResourceSystem &resources);
std::string SelectionSummary(const Unit &unit);
// Selection visuals + shortcut overlay builders.
float UnitHealthFraction(const Unit &unit); // hp / max, clamped to [0, 1]
std::vector<std::string> ShortcutHintLines(const HotkeyMap &hotkeys);
// Hover tooltip (world-space unit info): debounce state + pure updater.
// UpdateHoverTooltip returns true once the same unit has been hovered past
// delay seconds; a changed hover target resets the timer.
struct HoverTooltipState
{
    Entity hovered = kInvalidEntity;
    float time = 0.0f;
};
constexpr float kHoverTooltipDelay = 0.4f;
bool UpdateHoverTooltip(HoverTooltipState &state, Entity hovered, float dt, float delay);
// Two-line stat block for a hovered unit (summary + stance/state). Pure,
// tested; Game draws the lines near the cursor.
std::vector<std::string> UnitTooltipLines(const Unit &unit);

// Screen-space panels (call after EndMode2D).
// Optional art draw 16px resource icons; nullptr keeps text-only
// (headless tests, rectangle fallback).
void DrawResourcePanel(const ResourceSystem &resources, const Art *art = nullptr);
// Factory production UI. Display order for the 7 buildable types
// (pure, tested); the panel below is immediate-mode raygui owned by main.
std::vector<UnitType> ProductionMenuOrder();
// Factory panel with per-type cost buttons (unaffordable disabled), queue
// count + progress, and a cancel-top button. hasFactory=false shows a
// "Need Factory" stub. Returns the clicked type, or kNoProductionClick.
// Call after EndMode2D.
constexpr int kNoProductionClick = -1;
int DrawProductionPanel(ResourceSystem &resources, ProductionQueue &queue, bool hasFactory);
// Named-slot readout (filled/empty via file existence). Pure layout,
// raygui calls; slot files live at SaveSlotPath.
void DrawSaveSlots();
void DrawSelectionPanel(Registry &registry,
                        const Art *art = nullptr); // non-const: SelectedUnit queries selection
// QoL auto-repair panel: global checkbox + rate-cap slider operating
// directly on the caller's toggle state (immediate-mode, like the factory
// panel). Call after EndMode2D.
void DrawRepairPanel(bool *enabled, float *capFraction);
// QoL idle-select buttons ("Workers (N)" / "Army (N)") with live counts.
// Clicking selects that idle subset (replacing selection); greyed when 0.
// Call after EndMode2D.
void DrawIdleButtons(Registry &registry, int teamID);
// QoL control-group strip (10 boxes: member counts, selection + auto-add
// highlight). Pure scan + draw calls; call after EndMode2D.
void DrawControlGroupStrip(Registry &registry, int teamID, int autoAddGroupBit,
                           const Art *art = nullptr);
