// Unit tests for launch-hardening data-root resolution (DataRoot.h).
// PickDataRoot is pure (existence checks injected), so the search order
// is testable headless; the raylib DirectoryExists/ChangeDirectory calls
// only run at the Game::Init call site.

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
    // --- CWD already resolves data/: no change, whatever the exe dir holds ---
    gExistingDirs = { "C:/game/bin/data" };
    CC_CHECK(PickDataRoot(true, "C:/game/bin/", FakeDirExists) == nullptr);

    // --- exe dir hit wins without climbing ---
    gExistingDirs = { "C:/game/bin/data", "C:/game/data" };
    CC_CHECK(std::string(PickDataRoot(false, "C:/game/bin/", FakeDirExists)) ==
             "C:/game/bin/data");

    // --- missing levels skipped, deeper hit returned ---
    gExistingDirs = { "C:/game/bin/../../data" };
    CC_CHECK(std::string(PickDataRoot(false, "C:/game/bin/", FakeDirExists)) ==
             "C:/game/bin/../../data");

    // --- nothing found: no change (game proceeds, data I/O fails loudly) ---
    gExistingDirs.clear();
    CC_CHECK(PickDataRoot(false, "C:/game/bin/", FakeDirExists) == nullptr);

    // --- null guards never crash ---
    CC_CHECK(PickDataRoot(false, nullptr, FakeDirExists) == nullptr);
    CC_CHECK(PickDataRoot(false, "C:/game/bin/", nullptr) == nullptr);
    gExistingDirs.clear();
}
