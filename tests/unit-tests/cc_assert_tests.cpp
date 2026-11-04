// Unit tests for the CC_ASSERT profile behavior.
// A firing assert aborts the process, so the negative case can only be
// checked where the macro is compiled out: the #ifdef below survives in
// Release sources and vanishes entirely from Debug sources.

#include "test_harness.h"

#include "core/CcAssert.h"

void RunCcAssertTests()
{
	CC_ASSERT(true);
	CC_CHECK(true);

#ifndef NDEBUG
	// Frame lines show the basename only; both separators have to strip.
	CC_CHECK(std::string(cc_assert_detail::BaseName("src\\game\\core\\Game.cpp")) == "Game.cpp");
	CC_CHECK(std::string(cc_assert_detail::BaseName("src/game/core/Game.cpp")) == "Game.cpp");
	CC_CHECK(std::string(cc_assert_detail::BaseName("Game.cpp")) == "Game.cpp");
#endif

#ifdef NDEBUG
	CC_ASSERT(false); // must be compiled out in Release; aborts if live
	CC_CHECK(true);
#endif
}
