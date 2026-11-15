#pragma once

#include "core/Game.h"

class Registry;
class TileMap;

// Test-only subclass exposing the Game internals the Tier-1 harness drives;
// lives in the e2e project so the game build carries no harness.
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
