#pragma once

#include <string>
#include <vector>

#include "Registry.h"      // SelectedUnit lookup
#include "ResourceSystem.h" // stockpile counters
#include "Unit.h"          // selection summary

class Art; // fwd-decl (Hud.cpp includes Art.h for icons)

// M6 Goal 2: HUD panels. Text content is built by pure functions (tested);
// the Draw* wrappers below are thin raygui calls owned by main.cpp.

const char *UnitTypeName(UnitType type);
std::string FormatResources(const ResourceSystem &resources);
std::string SelectionSummary(const Unit &unit);
// M6 Goal 4: selection visuals + shortcut overlay builders.
float UnitHealthFraction(const Unit &unit); // hp / max, clamped to [0, 1]
std::vector<std::string> ShortcutHintLines();

// Screen-space panels (call after EndMode2D).
// M12: optional art draws 16px resource icons; nullptr keeps text-only
// (headless tests, rectangle fallback).
void DrawResourcePanel(const ResourceSystem &resources, const Art *art = nullptr);
void DrawSelectionPanel(Registry &registry); // non-const: SelectedUnit queries selection
