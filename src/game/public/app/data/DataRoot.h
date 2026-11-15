#pragma once

#include <cstdio>

inline const char *PickDataRoot(bool cwdHasData, const char *appDir,
								bool (*dirExists)(const char *))
{
	if (cwdHasData || appDir == nullptr || dirExists == nullptr)
	{
		return nullptr;
	}
	static char target[512];
	static const char *const suffixes[] = { "data", "../data", "../../data", "../../../data" };
	for (const char *suffix : suffixes)
	{
		std::snprintf(target, sizeof(target), "%s%s", appDir, suffix);
		if (dirExists(target))
		{
			return target;
		}
	}
	return nullptr;
}
