// Unit tests for selection visuals + shortcut overlay builders.

#include "test_harness.h"

#include "Hotkeys.h"
#include "Hud.h"
#include "UnitStats.h"

#include <set>

void RunSelectionVisualTests()
{
    // --- health fraction: full / half / empty / overkill / negative ---
    Unit full;
    full.type = UnitType::LightTank;
    full.health = BaseStats(UnitType::LightTank).health;
    CC_CHECK(UnitHealthFraction(full) == 1.0f);

    Unit half = full;
    half.health = BaseStats(UnitType::LightTank).health / 2.0f;
    CC_CHECK(CcNear(UnitHealthFraction(half), 0.5f));

    Unit dead = full;
    dead.health = 0.0f;
    CC_CHECK(UnitHealthFraction(dead) == 0.0f);

    Unit overkill = full;
    overkill.health = -25.0f;
    CC_CHECK(UnitHealthFraction(overkill) == 0.0f);

    Unit overheal = full;
    overheal.health = BaseStats(UnitType::LightTank).health * 1.5f;
    CC_CHECK(UnitHealthFraction(overheal) == 1.0f);

    // --- shortcut hints: one line per binding, no duplicates ---
    const HotkeyMap defaultHotkeys;
    const std::vector<std::string> hints = ShortcutHintLines(defaultHotkeys);
    CC_CHECK(!hints.empty());
    std::set<std::string> unique(hints.begin(), hints.end());
    CC_CHECK(unique.size() == hints.size());
    bool hasPause = false;
    bool hasHintsToggle = false;
    for (const std::string &line : hints)
    {
        hasPause = hasPause || line.find('P') != std::string::npos;
        hasHintsToggle = hasHintsToggle || line.find("F1") != std::string::npos;
    }
    CC_CHECK(hasPause);
    CC_CHECK(hasHintsToggle);
    // --- live overlay: rebinding an action updates its hint line ---
    HotkeyMap rebound = defaultHotkeys;
    rebound.Rebind("AttackMove", KEY_Q);
    const std::vector<std::string> reboundHints = ShortcutHintLines(rebound);
    CC_CHECK(reboundHints.size() == hints.size());
    bool hasNewBinding = false;
    bool hasOldBinding = false;
    for (const std::string &line : reboundHints)
    {
        hasNewBinding = hasNewBinding || line == "Q AttackMove";
        hasOldBinding = hasOldBinding || line == "A AttackMove";
    }
    CC_CHECK(hasNewBinding);
    CC_CHECK(!hasOldBinding);

    // --- hover tooltip debounce: new target resets, hold past delay shows ---
    HoverTooltipState tip;
    Registry tipReg;
    Entity u1 = tipReg.Create();
    Unit uu1;
    uu1.type = UnitType::RifleInfantry;
    uu1.health = BaseStats(UnitType::RifleInfantry).health;
    tipReg.Add(u1, uu1);
    Entity u2 = tipReg.Create();
    Unit uu2 = uu1;
    tipReg.Add(u2, uu2);
    CC_CHECK(!UpdateHoverTooltip(tip, u1, 0.1f, kHoverTooltipDelay));
    CC_CHECK(!UpdateHoverTooltip(tip, u1, 0.1f, kHoverTooltipDelay)); // 0.2s < delay
    CC_CHECK(!UpdateHoverTooltip(tip, u2, 0.1f, kHoverTooltipDelay)); // new target resets
    CC_CHECK(!UpdateHoverTooltip(tip, u2, 0.35f, kHoverTooltipDelay)); // 0.35s < delay
    CC_CHECK(UpdateHoverTooltip(tip, u2, 0.1f, kHoverTooltipDelay));  // 0.45s >= delay
    CC_CHECK(!UpdateHoverTooltip(tip, kInvalidEntity, 1.0f, kHoverTooltipDelay)); // lost: hides

    // --- tooltip lines: summary + stance/state ---
    const std::vector<std::string> tipLines = UnitTooltipLines(uu1);
    CC_CHECK(tipLines.size() == 2);
    CC_CHECK(tipLines[0] == SelectionSummary(uu1));
    CC_CHECK(tipLines[1] == "Guard, Idle");
}
