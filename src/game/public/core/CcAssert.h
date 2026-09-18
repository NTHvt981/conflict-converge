#pragma once

#include <cassert>

// Central assertion macro. Active in Debug builds, compiled out in
// Release (NDEBUG comes from the Release profile in premake5.lua). Use for
// invariant checks in game code ( tile math, unit state, economy);
// never for validation of external input (save files, user actions).
//
// NOTE: this file must NOT be named Assert.h — on Windows' case-insensitive
// filesystem it would shadow the CRT <assert.h> for every TU whose include
// path contains src/game/public, so <cassert> would never define the assert
// macro (MSVC C3861). Same trap as Math.h vs <math.h>. Hence CcAssert.h.

#ifdef NDEBUG
#define CC_ASSERT(cond) ((void)0)
#else
#define CC_ASSERT(cond) assert(cond)
#endif
