// main.cpp - RTS Game Entry Point (thin: Game owns state + loop).
#include "core/Game.h"

int main(void)
{
    Game game;
    return game.Run();
}
