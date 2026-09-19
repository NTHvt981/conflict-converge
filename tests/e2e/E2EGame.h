#pragma once

#include "Game.h"

// Test-only subclass: exposes the Game internals the Tier-1 harness drives
// directly. Lives in the e2e project so Game itself carries no test seam;
// the base grants it protected access to the match lifecycle only.
class E2EGame : public Game
{
public:
    MenuFlow &E2EMenu();
    Registry &E2ERegistry();
    TileMap &E2EMap();
    bool IsWorldActive() const;
    // SelectedMap + difficulty -> StartMatch; false when nothing selected.
    bool E2EStartSelectedMatch();
    void E2EQuitToMenu();
};
