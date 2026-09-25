#pragma once

#include "raylib.h"

#include "GameCamera.h"
#include "Shortcuts.h"
#include "Subsystem.h"

// Single per-frame polling point for input: WASD camera pan, mouse click
// edges, and shortcut actions. Game code binds actions once, calls Update
// every frame, then reads the snapshot — no raw raylib input calls outside
// this class. Snapshot is injectable so tests cover routing headless.

class InputManager : public Subsystem
{
public:
    ShortcutRegistry shortcuts;

    void Update(GameCamera &camera, float cameraSpeedPixelsPerSec, float dtSeconds);

    // Snapshot from live raylib state.
    void PollLive();

    // Snapshot from explicit values (tests, scripted input).
    void Snapshot(Vector2 mouseScreenPos, bool leftPressed, bool rightPressed, float wheelDelta = 0.0f,
                  bool shiftDown = false, bool leftDown = false, bool rightDown = false,
                  bool ctrlDown = false, double nowSeconds = 0.0);

    Vector2 MouseScreen() const;
    // Frame-to-frame mouse movement in screen px (for drag-pan); zero on the
    // first snapshot.
    Vector2 MouseDeltaScreen() const;
    bool LeftPressed() const;
    bool LeftDown() const;
    bool RightPressed() const;
    bool RightDown() const;
    float WheelDelta() const; // mouse wheel steps this frame
    bool ShiftDown() const;   // either shift key held
    bool CtrlDown() const;    // either control key held
    bool DoubleClicked() const; // edge: second left press within 0.35s + 8px

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
