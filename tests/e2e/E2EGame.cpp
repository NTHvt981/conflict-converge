#include "E2EGame.h"

#include "core/Registry.h"
#include "world/TileMap.h"

MenuFlow &E2EGame::E2EMenu()
{
	return menu_;
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
	return worldActive_;
}

bool E2EGame::E2EStartSelectedMatch()
{
	if (const MapEntry *sel = menu_.setup.SelectedMap(); sel != nullptr)
	{
		if (menu_.StartMatch())
		{
			StartMatch(sel->path, menu_.setup.difficulty);
			return true;
		}
	}
	return false;
}

void E2EGame::E2EQuitToMenu()
{
	QuitToMenu();
}
