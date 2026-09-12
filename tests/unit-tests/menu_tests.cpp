// Unit tests for the M6 Goal 3 menu system (state transitions + aliveness).

#include "test_harness.h"

#include "Menu.h"

void RunMenuTests()
{
    // --- pause toggle only touches Playing/Paused ---
    MenuFlow menu;
    CC_CHECK(menu.state == MenuState::Playing);
    menu.TogglePause();
    CC_CHECK(menu.state == MenuState::Paused);
    menu.TogglePause();
    CC_CHECK(menu.state == MenuState::Playing);

    menu.state = MenuState::GameOver;
    menu.TogglePause();
    CC_CHECK(menu.state == MenuState::GameOver);
    menu.state = MenuState::Victory;
    menu.TogglePause();
    CC_CHECK(menu.state == MenuState::Victory);

    // --- outcome matrix; player loss wins ties ---
    menu.state = MenuState::Playing;
    menu.ShowOutcome(true, true);
    CC_CHECK(menu.state == MenuState::Playing);
    menu.ShowOutcome(true, false);
    CC_CHECK(menu.state == MenuState::Victory);
    menu.state = MenuState::Playing;
    menu.ShowOutcome(false, true);
    CC_CHECK(menu.state == MenuState::GameOver);
    menu.state = MenuState::Playing;
    menu.ShowOutcome(false, false);
    CC_CHECK(menu.state == MenuState::GameOver);

    // --- defaults ---
    MenuFlow fresh;
    CC_CHECK(fresh.settings.cameraSpeed == 400.0f);
    CC_CHECK(fresh.settings.showMinimap);
    CC_CHECK(!fresh.quitRequested);

    // --- team aliveness ignores corpses and other teams ---
    Registry registry;
    CC_CHECK(!TeamHasUnits(registry, 0));

    Unit ally;
    ally.teamID = 0;
    ally.health = 10.0f;
    const Entity allyId = registry.Create();
    registry.Add(allyId, ally);
    CC_CHECK(TeamHasUnits(registry, 0));
    CC_CHECK(!TeamHasUnits(registry, 1));

    Unit *stored = registry.Get<Unit>(allyId);
    stored->health = 0.0f; // corpse: side is out
    CC_CHECK(!TeamHasUnits(registry, 0));

    Unit foe;
    foe.teamID = 1;
    foe.health = 50.0f;
    registry.Add(registry.Create(), foe);
    CC_CHECK(TeamHasUnits(registry, 1));
    CC_CHECK(!TeamHasUnits(registry, 0));
}
