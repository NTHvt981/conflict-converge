#pragma once

#include <cstdio> // snprintf for candidate paths

// Launch hardening: the game resolves data/ relative to CWD (repo root
// under F5, exe dir for direct runs thanks to the postbuild copy). A
// foreign CWD (terminal elsewhere, bare shortcut) breaks every data path.
// PickDataRoot finds the directory to chdir to so data/ resolves again,
// mirroring extras-c/path_utils' search locations (CWD, exe dir, up to 3
// above it) but with parent-of-data semantics: upstream's
// SearchAndSetResourceDir chdirs INTO data/, which would break our
// data/-prefixed paths, so it cannot be called directly.
//
// Pure and headless-tested: existence checks are injected, raylib's
// DirectoryExists/ChangeDirectory/GetApplicationDirectory only run at the
// Game::Init call site. Returns nullptr when CWD already resolves data/
// (no change needed); otherwise a pointer to an internal static buffer
// holding the target dir (use immediately, like TextFormat).
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
