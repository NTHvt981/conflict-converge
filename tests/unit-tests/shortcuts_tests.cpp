// Unit tests for the shortcut registry (bind, route, unbind).
// PollAndFire polls live raylib input; routing is covered via Fire.

#include "test_harness.h"

#include "app/input/Shortcuts.h"

void RunShortcutTests()
{
	ShortcutRegistry shortcuts;
	CC_CHECK(!shortcuts.Has(1));

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

	CC_CHECK(!shortcuts.Fire(99));
	CC_CHECK(callsA == 1);

	shortcuts.Bind(1, [&] { callsA += 10; });
	shortcuts.Fire(1);
	CC_CHECK(callsA == 11);

	shortcuts.Unbind(1);
	CC_CHECK(!shortcuts.Has(1));
	CC_CHECK(!shortcuts.Fire(1));
	CC_CHECK(shortcuts.Has(2));
	shortcuts.Unbind(1); // absent unbind is a no-op
	CC_CHECK(true);

	shortcuts.Clear();
	CC_CHECK(!shortcuts.Has(2));
	CC_CHECK(!shortcuts.Fire(2));

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

	// PollAndFire snapshots keys before firing; FireWithShift is a single live
	// table lookup, so table surgery mid-action cannot invalidate anything.
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

	ShortcutRegistry gate;
	int gated = 0;
	gate.Bind(5, [&] { ++gated; });
	gate.BindChord(5, [&] { ++gated; });
	gate.SetEnabled(false);
	CC_CHECK(!gate.Enabled());
	CC_CHECK(!gate.Fire(5));
	CC_CHECK(!gate.FireChord(5));
	CC_CHECK(!gate.FireWithShift(5, false));
	CC_CHECK(!gate.FireWithShift(5, true));
	CC_CHECK(gated == 0);
	gate.SetEnabled(true);
	CC_CHECK(gate.Enabled());
	CC_CHECK(gate.Fire(5));
	CC_CHECK(gated == 1);
}
