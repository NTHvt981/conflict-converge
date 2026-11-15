#pragma once

#include "raylib.h"

#include "app/ui/GameCamera.h"
#include "app/input/Shortcuts.h"
#include "core/Subsystem.h"

// The only place raw raylib input is read: one poll per frame, then game code
// reads the snapshot. Snapshot is injectable so routing is testable headless.

class InputManager : public Subsystem
{
public:
	ShortcutRegistry shortcuts;

	void Update(GameCamera &camera, float cameraSpeedPixelsPerSec, float dtSeconds);

	// Snapshot from live raylib state.
	void PollLive();

	void Snapshot(Vector2 mouseScreenPos, bool leftPressed, bool rightPressed, float wheelDelta = 0.0f,
				  bool shiftDown = false, bool leftDown = false, bool rightDown = false,
				  bool ctrlDown = false, double nowSeconds = 0.0);

	Vector2 MouseScreen() const;
	// Screen px since the previous poll; zero on the first.
	Vector2 MouseDeltaScreen() const;
	bool LeftPressed() const;
	bool LeftDown() const;
	bool RightPressed() const;
	bool RightDown() const;
	float WheelDelta() const; // mouse wheel steps this frame
	bool ShiftDown() const;
	bool CtrlDown() const;
	bool DoubleClicked() const; // edge: second left press within 0.35s and 8px

	Vector2 MouseWorld(const GameCamera &camera) const;

private:
	Vector2 mouseScreen_ = {};
	Vector2 prevMouseScreen_ = {};
	Vector2 mouseDelta_ = {};
	bool hasPrevMouse_ = false;
	bool leftPressed_ = false;
	bool rightPressed_ = false;
	float wheelDelta_ = 0.0f;
	bool shiftDown_ = false;
	bool leftDown_ = false;
	bool rightDown_ = false;
	bool ctrlDown_ = false;
	double lastLeftClickTime_ = -1.0;
	Vector2 lastLeftClickPos_ = {};
	bool doubleClicked_ = false;
};
