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
