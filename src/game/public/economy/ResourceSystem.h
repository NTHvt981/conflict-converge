#pragma once

#include "Subsystem.h"

// Iron and oil ledger, uncapped.

class ResourceSystem : public Subsystem
{
public:
    long iron = 0;
    long oil = 0;

    void AddIron(long amount);
    void AddOil(long amount);
    bool TrySpend(long ironCost, long oilCost);

    void TickIncome(float ironPerSecond, float oilPerSecond, float dt);

    float IronCarry() const;
    float OilCarry() const;
    void SetCarry(float ironCarry, float oilCarry);

private:
    float ironCarry_ = 0.0f;
    float oilCarry_ = 0.0f;
};
