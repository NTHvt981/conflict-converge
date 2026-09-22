#include "E2EGame.h"

MenuFlow &E2EGame::E2EMenu()
{
    return menu;
}

Registry &E2EGame::E2ERegistry()
{
    return world_.Get<Registry>();
}

TileMap &E2EGame::E2EMap()
{
    return world_.Get<TileMap>();
}

bool E2EGame::IsWorldActive() const
{
    return worldActive;
}

bool E2EGame::E2EStartSelectedMatch()
{
    if (const MapEntry *sel = menu.setup.SelectedMap(); sel != nullptr)
    {
        if (menu.StartMatch())
        {
            StartMatch(sel->path, menu.setup.difficulty);
            return true;
        }
    }
    return false;
}

void E2EGame::E2EQuitToMenu()
{
    QuitToMenu();
}
