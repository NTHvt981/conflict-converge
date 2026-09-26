// Unit tests for base income (TickIncome carry, no cap).

#include "test_harness.h"

#include "economy/ResourceSystem.h"

void RunIncomeTests()
{
    // --- whole-unit banking, no cap ---
    ResourceSystem resources;
    resources.TickIncome(5.0f, 3.0f, 1.0f);
    CC_CHECK(resources.iron == 5);
    CC_CHECK(resources.oil == 3);

    // --- fractional carry across ticks ---
    ResourceSystem trickle;
    trickle.TickIncome(0.5f, 0.25f, 1.0f);
    CC_CHECK(trickle.iron == 0 && trickle.oil == 0);
    trickle.TickIncome(0.5f, 0.25f, 1.0f);
    CC_CHECK(trickle.iron == 1 && trickle.oil == 0);
    trickle.TickIncome(0.5f, 0.25f, 1.0f);
    trickle.TickIncome(0.5f, 0.25f, 1.0f);
    CC_CHECK(trickle.iron == 2 && trickle.oil == 1);

    // --- variable dt accumulates the same total ---
    ResourceSystem paced;
    for (int i = 0; i < 60; ++i)
    {
        paced.TickIncome(60.0f, 0.0f, 1.0f / 60.0f);
    }
    CC_CHECK(paced.iron == 60);

    // --- degenerate inputs never move balances ---
    ResourceSystem guarded;
    guarded.TickIncome(10.0f, 10.0f, 0.0f);
    guarded.TickIncome(10.0f, 10.0f, -1.0f);
    guarded.TickIncome(-5.0f, -5.0f, 1.0f);
    CC_CHECK(guarded.iron == 0 && guarded.oil == 0);

    // --- income composes with spending ---
    ResourceSystem economy;
    economy.AddIron(100);
    economy.TickIncome(10.0f, 0.0f, 1.0f);
    CC_CHECK(economy.TrySpend(110, 0));
    CC_CHECK(economy.iron == 0);
}
