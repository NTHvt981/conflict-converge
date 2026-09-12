#include "Menu.h"

void MenuFlow::TogglePause()
{
    if (state == MenuState::Playing)
    {
        state = MenuState::Paused;
    }
    else if (state == MenuState::Paused)
    {
        state = MenuState::Playing;
    }
}

void MenuFlow::ShowOutcome(bool playerAlive, bool enemyAlive)
{
    if (!playerAlive)
    {
        state = MenuState::GameOver;
    }
    else if (!enemyAlive)
    {
        state = MenuState::Victory;
    }
}

bool TeamHasUnits(const Registry &registry, int teamID)
{
    bool found = false;
    registry.Each<Unit>([&](Entity, const Unit &unit) {
        if (unit.teamID == teamID && unit.health > 0.0f)
        {
            found = true;
        }
    });
    return found;
}
