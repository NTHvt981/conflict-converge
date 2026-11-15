// Unit tests for data-root resolution (DataRoot.h). PickDataRoot is pure
// (existence checks injected), so the search order is testable headless.

#include "test_harness.h"

#include "app/data/DataRoot.h"

#include <string>
#include <vector>

namespace
{

std::vector<std::string> gExistingDirs;

bool FakeDirExists(const char *path)
{
	for (const std::string &dir : gExistingDirs)
	{
		if (dir == path)
		{
			return true;
		}
	}
	return false;
}

} // namespace

void RunDataRootTests()
{
	gExistingDirs = { "C:/game/bin/data" };
	CC_CHECK(PickDataRoot(true, "C:/game/bin/", FakeDirExists) == nullptr);

	gExistingDirs = { "C:/game/bin/data", "C:/game/data" };
	CC_CHECK(std::string(PickDataRoot(false, "C:/game/bin/", FakeDirExists)) ==
			 "C:/game/bin/data");

	gExistingDirs = { "C:/game/bin/../../data" };
	CC_CHECK(std::string(PickDataRoot(false, "C:/game/bin/", FakeDirExists)) ==
			 "C:/game/bin/../../data");

	gExistingDirs.clear();
	CC_CHECK(PickDataRoot(false, "C:/game/bin/", FakeDirExists) == nullptr);

	CC_CHECK(PickDataRoot(false, nullptr, FakeDirExists) == nullptr);
	CC_CHECK(PickDataRoot(false, "C:/game/bin/", nullptr) == nullptr);
	gExistingDirs.clear();
}
