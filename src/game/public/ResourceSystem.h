#pragma once

// Forward-declared API shape for M5 (Resource & Economy).
// Tracks iron and oil with no cap. Gathering/production logic lands in M5.

class ResourceSystem
{
public:
    long iron = 0;
    long oil = 0;

    void AddIron(long amount);
    void AddOil(long amount);
    bool TrySpend(long ironCost, long oilCost);
};
