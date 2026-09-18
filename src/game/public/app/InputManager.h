#pragma once

#include "raylib.h" // Vector2

#include "GameCamera.h" // WASD update + screen->world target
#include "Shortcuts.h"  // bound-key actions, polled per frame

// Input handling framework. Single per-frame polling point for
// everything gathered piecemeal: WASD camera pan, mouse click edges,
// and shortcut actions. Game code binds ShortcutRegistry actions once,
// calls Update every frame, then reads the snapshot — no raw raylib input
// calls outside this class.
//
// Snapshot is injectable so tests cover routing headless: PollLive reads
// raylib state, Update = WASD + shortcuts + PollLive.

class InputManager
{
public:
    ShortcutRegistry shortcuts;

    // Full per-frame pump: camera WASD, shortcut actions, mouse snapshot.
    void Update(GameCamera &camera, float cameraSpeedPixelsPerSec, float dtSeconds);

    // Snapshot from live raylib state (mouse position + click edges).
    void PollLive();

    // Snapshot from explicit values (tests, scripted input).
    void Snapshot(Vector2 mouseScreenPos, bool leftPressed, bool rightPressed, float wheelDelta = 0.0f,
                  bool shiftDown = false, bool leftDown = false, bool rightDown = false,
                  bool ctrlDown = false, double nowSeconds = 0.0);

    Vector2 MouseScreen() const;
    // Frame-to-frame mouse movement in screen px (for drag-pan). Zero on
    // the first snapshot (no previous position yet).
    Vector2 MouseDeltaScreen() const;
    bool LeftPressed() const; // edge: button went down this frame
    bool LeftDown() const;    // level: button held (drag-box gestures)
    bool RightPressed() const;
    bool RightDown() const;  // level: right button held (line-draw, right-drag pan)
    float WheelDelta() const; // mouse wheel steps this frame
    bool ShiftDown() const;   // either shift key held (slot load/save combos)
    bool CtrlDown() const;    // either control key held (QoL control groups)
    bool DoubleClicked() const; // edge: second left press within 0.35s + 8px

    // Snapshot mouse position under the given camera view.
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
    // QoL double-click select-type: last left-press time/pos; set by
    // Snapshot (PollLive passes the real GetTime, tests pass explicit).
    double lastLeftClickTime_ = -1.0;
    Vector2 lastLeftClickPos_ = {};
    bool doubleClicked_ = false;
};
