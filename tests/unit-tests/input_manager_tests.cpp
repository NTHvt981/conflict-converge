// Unit tests for the InputManager (snapshot routing).
// Update/PollLive read live raylib state; Snapshot covers the same paths.

#include "test_harness.h"

#include "app/input/InputManager.h"

void RunInputManagerTests()
{
	InputManager input;

	CC_CHECK(input.MouseScreen().x == 0.0f);
	CC_CHECK(!input.LeftPressed());
	CC_CHECK(!input.LeftDown());
	CC_CHECK(!input.RightPressed());

	input.Snapshot({ 100.0f, 50.0f }, true, false);
	CC_CHECK(input.MouseScreen().x == 100.0f);
	CC_CHECK(input.MouseScreen().y == 50.0f);
	CC_CHECK(input.LeftPressed());
	CC_CHECK(!input.RightPressed());

	input.Snapshot({ 10.0f, 20.0f }, false, true);
	CC_CHECK(!input.LeftPressed());
	CC_CHECK(input.RightPressed());

	GameCamera camera;
	camera.view.offset = { 400.0f, 225.0f };
	camera.view.zoom = 1.0f;
	camera.view.target = { 64.0f, 64.0f };
	input.Snapshot({ 400.0f, 225.0f }, false, false);
	Vector2 world = input.MouseWorld(camera);
	CC_CHECK(CcNear(world.x, 64.0f));
	CC_CHECK(CcNear(world.y, 64.0f));

	input.Snapshot({ 0.0f, 0.0f }, false, false);
	CC_CHECK(!input.LeftPressed());
	CC_CHECK(!input.RightPressed());

	input.Snapshot({ 0.0f, 0.0f }, true, false, 0.0f, false, true);
	CC_CHECK(input.LeftPressed());
	CC_CHECK(input.LeftDown());
	input.Snapshot({ 0.0f, 0.0f }, false, false, 0.0f, false, true);
	CC_CHECK(!input.LeftPressed());
	CC_CHECK(input.LeftDown()); // still held: the drag continues
	input.Snapshot({ 0.0f, 0.0f }, false, false);
	CC_CHECK(!input.LeftPressed());
	CC_CHECK(!input.LeftDown()); // released

	CC_CHECK(!input.RightDown());
	input.Snapshot({ 0.0f, 0.0f }, false, true, 0.0f, false, false, true);
	CC_CHECK(input.RightPressed());
	CC_CHECK(input.RightDown());
	input.Snapshot({ 0.0f, 0.0f }, false, false, 0.0f, false, false, true);
	CC_CHECK(!input.RightPressed());
	CC_CHECK(input.RightDown()); // still held
	input.Snapshot({ 0.0f, 0.0f }, false, false);
	CC_CHECK(!input.RightDown()); // released

	CC_CHECK(!input.CtrlDown());
	input.Snapshot({ 0.0f, 0.0f }, false, false, 0.0f, false, false, false, true);
	CC_CHECK(input.CtrlDown());
	CC_CHECK(!input.ShiftDown());
	input.Snapshot({ 0.0f, 0.0f }, false, false);
	CC_CHECK(!input.CtrlDown());

	input.Snapshot({ 100.0f, 100.0f }, false, false);
	CC_CHECK(input.MouseDeltaScreen().x == 100.0f);
	CC_CHECK(input.MouseDeltaScreen().y == 100.0f);
	input.Snapshot({ 110.0f, 95.0f }, false, false);
	CC_CHECK(input.MouseDeltaScreen().x == 10.0f);
	CC_CHECK(input.MouseDeltaScreen().y == -5.0f);
	input.Snapshot({ 110.0f, 95.0f }, false, false);
	CC_CHECK(input.MouseDeltaScreen().x == 0.0f); // held still
	CC_CHECK(input.MouseDeltaScreen().y == 0.0f);
	// A fresh manager reports no jump on its very first snapshot.
	InputManager fresh;
	fresh.Snapshot({ 50.0f, 60.0f }, false, false);
	CC_CHECK(fresh.MouseDeltaScreen().x == 0.0f);
	CC_CHECK(fresh.MouseDeltaScreen().y == 0.0f);

	int halts = 0;
	input.shortcuts.Bind(32, [&] { ++halts; });
	CC_CHECK(input.shortcuts.Fire(32));
	CC_CHECK(halts == 1);

	InputManager clicks;
	clicks.Snapshot({ 200.0f, 200.0f }, true, false, 0.0f, false, false, false, false,
					 10.0);
	CC_CHECK(!clicks.DoubleClicked()); // first of a potential pair
	clicks.Snapshot({ 200.0f, 200.0f }, false, false, 0.0f, false, false, false, false,
					 10.2);
	CC_CHECK(!clicks.DoubleClicked()); // release: edge fires on press only
	clicks.Snapshot({ 203.0f, 201.0f }, true, false, 0.0f, false, false, false, false,
					 10.3);
	CC_CHECK(clicks.DoubleClicked()); // within 0.35s + 8px
	clicks.Snapshot({ 203.0f, 201.0f }, false, false, 0.0f, false, false, false,
					 false, 10.4);
	CC_CHECK(!clicks.DoubleClicked()); // consumed: edge lasts one frame

	InputManager slow;
	slow.Snapshot({ 50.0f, 50.0f }, true, false, 0.0f, false, false, false, false, 1.0);
	slow.Snapshot({ 50.0f, 50.0f }, true, false, 0.0f, false, false, false, false, 2.0);
	CC_CHECK(!slow.DoubleClicked()); // 1s apart: too slow
	InputManager far;
	far.Snapshot({ 50.0f, 50.0f }, true, false, 0.0f, false, false, false, false, 5.0);
	far.Snapshot({ 200.0f, 200.0f }, true, false, 0.0f, false, false, false, false,
				  5.1);
	CC_CHECK(!far.DoubleClicked()); // too far apart
}
