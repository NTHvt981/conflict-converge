#pragma once

#include "Registry.h" // TeamHasUnits query
#include "Unit.h"     // health aliveness check

// M6 Goal 3: menu system. MenuFlow owns the top-level game state; main.cpp
// skips the sim while paused and draws outcome/settings windows. Text and
// transitions are pure (tested); the raygui windows live in main.cpp.

enum class MenuState
{
    Playing,
    Paused,
    GameOver,
    Victory
};

struct MenuSettings
{
    float cameraSpeed = 400.0f;
    bool showMinimap = true;
};

struct MenuFlow
{
    MenuState state = MenuState::Playing;
    MenuSettings settings;
    bool quitRequested = false;

    // Playing <-> Paused only; outcome screens are terminal until quit.
    void TogglePause();
    // Decide terminal states from team aliveness (call each frame while Playing).
    // Player loss takes priority when both sides are wiped.
    void ShowOutcome(bool playerAlive, bool enemyAlive);
};

// Any living unit (health > 0) on the given team keeps that side in the game.
bool TeamHasUnits(const Registry &registry, int teamID);
