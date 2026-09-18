// Unit tests for the shortcut registry (bind, route, unbind).
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

    // --- FireWithShift keeps poll-time chord priority, headlessly ---
    ShortcutRegistry chords;
    int plain = 0;
    int chord = 0;
    chords.Bind(7, [&] { ++plain; });
    chords.BindChord(7, [&] { ++chord; });
    CC_CHECK(chords.FireWithShift(7, false));
    CC_CHECK(plain == 1 && chord == 0);
    CC_CHECK(chords.FireWithShift(7, true));
    CC_CHECK(plain == 1 && chord == 1);
    chords.Bind(8, [&] { ++plain; }); // plain-only still fires under Shift
    CC_CHECK(chords.FireWithShift(8, true));
    CC_CHECK(plain == 2);
    CC_CHECK(!chords.FireWithShift(99, false));

    // --- re-entrant Bind/Unbind/Clear around firing is safe ---
    // PollAndFire snapshots keys for exactly this reason; FireWithShift
    // itself is a single live-table lookup, so table surgery mid-action
    // cannot invalidate anything.
    ShortcutRegistry mut;
    int a = 0;
    int b = 0;
    mut.Bind(1, [&] {
        ++a;
        mut.Unbind(1);
        mut.Bind(2, [&] { ++b; });
    });
    CC_CHECK(mut.FireWithShift(1, false));
    CC_CHECK(a == 1);
    CC_CHECK(!mut.Has(1));
    CC_CHECK(mut.FireWithShift(2, false));
    CC_CHECK(b == 1);
    mut.Bind(3, [&] {
        ++a;
        mut.Clear();
    });
    mut.Bind(4, [&] { ++a; });
    CC_CHECK(mut.FireWithShift(3, false));
    CC_CHECK(a == 2);
    CC_CHECK(!mut.Has(4)); // cleared mid-flight
    CC_CHECK(!mut.FireWithShift(4, false));

    // --- rehash storm mid-action: 200 binds, then fire through ---
    ShortcutRegistry storm;
    int fired = 0;
    storm.Bind(1, [&] {
        ++fired;
        for (int i = 100; i < 300; ++i)
        {
            storm.Bind(i, [&] { ++fired; });
        }
    });
    CC_CHECK(storm.FireWithShift(1, false));
    CC_CHECK(fired == 1);
    CC_CHECK(storm.FireWithShift(100, false));
    CC_CHECK(fired == 2);
}
