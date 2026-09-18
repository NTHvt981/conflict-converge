#pragma once

// Iron and oil ledger, uncapped. Spending and income ticks live here;
// gathering and production queues live in Nodes/Production/Building.

class ResourceSystem
{
public:
    long iron = 0;
    long oil = 0;

    void AddIron(long amount);
    void AddOil(long amount);
    bool TrySpend(long ironCost, long oilCost);

    // Base income generation. Banks whole units per tick,
    // carrying fractions forward; never caps (design: income has no maximum).
    void TickIncome(float ironPerSecond, float oilPerSecond, float dt);

    // Save/load support: fractional income banks (not visible in balances).
    float IronCarry() const;
    float OilCarry() const;
    void SetCarry(float ironCarry, float oilCarry);

private:
    float ironCarry_ = 0.0f;
    float oilCarry_ = 0.0f;
};
