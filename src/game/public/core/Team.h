#pragma once

enum class TeamId : int
{
	Any = -1,
	Player = 0,
	Enemy = 1
};

inline constexpr TeamId kAnyTeam = TeamId::Any;
inline constexpr TeamId kPlayerTeam = TeamId::Player;
inline constexpr TeamId kEnemyTeam = TeamId::Enemy;
