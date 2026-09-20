#include "test_harness.h"

#include "Cheats.h"

#include <cstring>

#if defined(CC_DEBUG)
DEFINE_CHEAT_WIDGET(TEST_TOGGLE, TOGGLE, false)
DEFINE_CHEAT_WIDGET(TEST_INT, INT, 7)
DEFINE_CHEAT_WIDGET(TEST_FLOAT, FLOAT, 1.5f)
DEFINE_CHEAT_WIDGET(TEST_BUTTON, BUTTON, false)
#endif

void RunCheatTests()
{
#if defined(CC_DEBUG)
    auto Find = [](const char *name) -> cc::cheat::CheatWidgetBase * {
        for (cc::cheat::CheatWidgetBase *widget : cc::cheat::CheatRegistry::Instance().Widgets())
        {
            if (std::strcmp(widget->Name(), name) == 0)
            {
                return widget;
            }
        }
        return nullptr;
    };

    CC_CHECK(TEST_TOGGLE.value() == false);
    CC_CHECK(TEST_INT.value() == 7);
    CC_CHECK(CcNear(TEST_FLOAT.value(), 1.5f));
    CC_CHECK(TEST_BUTTON.value() == false);

    CC_CHECK(Find("TEST_TOGGLE") != nullptr);
    CC_CHECK(Find("TEST_INT") != nullptr);
    CC_CHECK(Find("TEST_FLOAT") != nullptr);
    CC_CHECK(Find("TEST_BUTTON") != nullptr);

    CC_CHECK(std::strstr(TEST_TOGGLE.File(), "cheat_tests.cpp") != nullptr);
    CC_CHECK(TEST_TOGGLE.Kind() == cc::cheat::CheatWidgetKind::Toggle);
    CC_CHECK(TEST_INT.Kind() == cc::cheat::CheatWidgetKind::Int);
    CC_CHECK(TEST_FLOAT.Kind() == cc::cheat::CheatWidgetKind::Float);
    CC_CHECK(TEST_BUTTON.Kind() == cc::cheat::CheatWidgetKind::Button);

    TEST_INT.Set(42);
    CC_CHECK(TEST_INT.value() == 42);
    TEST_INT.Set(7);

    TEST_TOGGLE.Set(true);
    CC_CHECK(TEST_TOGGLE.value());
    TEST_TOGGLE.Set(false);

    CC_CHECK(Find("SHOW_UNIT_ID_TOGGLE") != nullptr);
    CC_CHECK(Find("SHOW_UNIT_ID_TOGGLE")->Kind() == cc::cheat::CheatWidgetKind::Toggle);

    int codeRan = 0;
    DEFINE_CHEAT_CODE(codeRan = 1;)
    CC_CHECK(codeRan == 1);
#else
    int codeRan = 0;
    DEFINE_CHEAT_CODE(codeRan = 1;)
    CC_CHECK(codeRan == 0);
#endif
}
