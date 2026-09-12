// Unit tests for the M2 Goal 5 shortcut registry (bind, route, unbind).
// PollAndFire polls live raylib input; routing is covered via Fire.

#include "test_harness.h"

#include "Shortcuts.h"

void RunShortcutTests()
{
    ShortcutRegistry shortcuts;
    CC_CHECK(!shortcuts.Has(1));

    // --- bind + fire routes to the right action, in order ---
    int callsA = 0;
    int callsB = 0;
    shortcuts.Bind(1, [&] { ++callsA; });
    shortcuts.Bind(2, [&] { ++callsB; });
    CC_CHECK(shortcuts.Has(1));
    CC_CHECK(shortcuts.Has(2));

    CC_CHECK(shortcuts.Fire(1));
    CC_CHECK(callsA == 1);
    CC_CHECK(callsB == 0);
    CC_CHECK(shortcuts.Fire(2));
    CC_CHECK(callsA == 1);
    CC_CHECK(callsB == 1);

    // --- unbound fire is a safe no-op ---
    CC_CHECK(!shortcuts.Fire(99));
    CC_CHECK(callsA == 1);

    // --- rebind replaces the action ---
    shortcuts.Bind(1, [&] { callsA += 10; });
    shortcuts.Fire(1);
    CC_CHECK(callsA == 11);

    // --- unbind + clear ---
    shortcuts.Unbind(1);
    CC_CHECK(!shortcuts.Has(1));
    CC_CHECK(!shortcuts.Fire(1));
    CC_CHECK(shortcuts.Has(2));
    shortcuts.Unbind(1); // absent unbind is a no-op
    CC_CHECK(true);

    shortcuts.Clear();
    CC_CHECK(!shortcuts.Has(2));
    CC_CHECK(!shortcuts.Fire(2));
}
