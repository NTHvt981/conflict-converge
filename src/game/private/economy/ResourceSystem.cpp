#include "ResourceSystem.h"

// Minimal ledger so the M3 UnitFactory can validate costs. Gathering,
// buildings, and the production queue arrive in M5.
void ResourceSystem::AddIron(long amount)
{
    iron += amount;
}

void ResourceSystem::AddOil(long amount)
{
    oil += amount;
}

bool ResourceSystem::TrySpend(long ironCost, long oilCost)
{
    if (ironCost < 0 || oilCost < 0)
    {
        return false;
    }
    if (iron < ironCost || oil < oilCost)
    {
        return false; // insufficient funds: balances untouched
    }
    iron -= ironCost;
    oil -= oilCost;
    return true;
}

void ResourceSystem::TickIncome(float ironPerSecond, float oilPerSecond, float dt)
{
    if (dt <= 0.0f)
    {
        return;
    }
    if (ironPerSecond < 0.0f)
    {
        ironPerSecond = 0.0f; // income never drains; spending goes through TrySpend
    }
    if (oilPerSecond < 0.0f)
    {
        oilPerSecond = 0.0f;
    }
    ironCarry_ += ironPerSecond * dt;
    oilCarry_ += oilPerSecond * dt;
    const long ironWhole = static_cast<long>(ironCarry_);
    const long oilWhole = static_cast<long>(oilCarry_);
    iron += ironWhole;
    oil += oilWhole;
    ironCarry_ -= static_cast<float>(ironWhole);
    oilCarry_ -= static_cast<float>(oilWhole);
}

float ResourceSystem::IronCarry() const
{
    return ironCarry_;
}

float ResourceSystem::OilCarry() const
{
    return oilCarry_;
}

void ResourceSystem::SetCarry(float ironCarry, float oilCarry)
{
    ironCarry_ = ironCarry;
    oilCarry_ = oilCarry;
}
