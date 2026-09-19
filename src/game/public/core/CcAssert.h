#pragma once

#include <cassert>

// Central assertion macro: active in Debug, compiled out under NDEBUG.
// Use for invariant checks, never external input validation.

#ifdef NDEBUG
#define CC_ASSERT(cond) ((void)0)
#else
#define CC_ASSERT(cond) assert(cond)
#endif
