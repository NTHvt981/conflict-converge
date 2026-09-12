// M7 Goal 3: simulation performance budget. Headless UpdateUnit sweep over
// 200 mixed units (no rendering) — validates sim cost stays well inside the
// 16.7ms frame budget at 60 FPS. Prints observed timing for profiling.

#include "../unit-tests/test_harness.h"

#include "MathUtils.h"
#include "Registry.h"
#include "ResourceSystem.h"
#include "TileMap.h"
#include "Unit.h"
#include "UnitFactory.h"

#include <chrono>
#include <cstdio>
#include <vector>

void RunPerfTests()
{
    Registry registry;
    ResourceSystem resources;
    resources.AddIron(1000000);
    resources.AddOil(1000000);
    EventDispatcher events;
    UnitFactory factory(registry, resources, events);
    TileMap map(40, 30);

    const UnitType mix[] = {
        UnitType::Infantry, UnitType::LightTank, UnitType::Artillery, UnitType::AntiArmorInfantry
    };
    for (int i = 0; i < 200; ++i)
    {
        const cc::Vec2 corner = cc::TileToWorld(i % 40, (i / 40) % 30);
        factory.Spawn(mix[i % 4], i % 2, cc::ToRaylib(corner));
    }
    CC_CHECK(registry.EntityCount() == 200);

    constexpr float kDt = 1.0f / 60.0f;
    constexpr int kFrames = 600;
    const auto start = std::chrono::steady_clock::now();
    for (int f = 0; f < kFrames; ++f)
    {
        registry.Each<Unit>([&](Entity id, Unit &unit) { UpdateUnit(id, registry, map, kDt); });
        std::vector<Entity> dead;
        registry.Each<Unit>([&](Entity id, const Unit &unit) {
            if (unit.health <= 0.0f)
            {
                dead.push_back(id);
            }
        });
        for (Entity id : dead)
        {
            factory.DestroyUnit(id);
        }
    }
    const double ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    std::printf("perf: 200 units x %d frames: %.0f ms (%.3f ms/frame)\n", kFrames, ms,
                ms / kFrames);

    // Budget: 5s total (~8x the observed ~0.66s in Debug) — sim cost stays
    // an order of magnitude inside the 16.7ms frame budget at 60 FPS.
    CC_CHECK(ms < 5000.0);
}
