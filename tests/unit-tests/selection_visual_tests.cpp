// Unit tests for M6 Goal 4 selection visuals + shortcut overlay builders.

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
}
