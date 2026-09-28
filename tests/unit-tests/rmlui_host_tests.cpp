#include "test_harness.h"

#include "app/ui/RmlUiHost.h"

void RunRmlUiHostTests()
{
    CC_CHECK(CcNear(RmlUiHost::AutoDpRatio(1280, 720, 1.0f), 1.0f));
    CC_CHECK(CcNear(RmlUiHost::AutoDpRatio(800, 600, 1.0f), 0.625f));
    CC_CHECK(CcNear(RmlUiHost::AutoDpRatio(1600, 360, 1.0f), 0.5f));
    CC_CHECK(CcNear(RmlUiHost::AutoDpRatio(1920, 1080, 1.0f), 1.5f));
    CC_CHECK(CcNear(RmlUiHost::AutoDpRatio(1280, 720, 2.0f), 2.0f));
    CC_CHECK(CcNear(RmlUiHost::AutoDpRatio(3840, 2160, 2.0f), 3.0f));
    CC_CHECK(CcNear(RmlUiHost::AutoDpRatio(320, 200, 0.75f), 0.5f));
}
