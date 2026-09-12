// Unit tests for the M1 Goal 7 CC_ASSERT profile behavior.
// A firing assert aborts the process, so the negative case can only be
// checked where the macro is compiled out: the #ifdef below survives in
// Release sources and vanishes entirely from Debug sources.

#include "test_harness.h"

#include "CcAssert.h"

void RunCcAssertTests()
{
    CC_ASSERT(true);
    CC_CHECK(true);

#ifdef NDEBUG
    CC_ASSERT(false); // must be compiled out in Release; aborts if live
    CC_CHECK(true);
#endif
}
