#pragma once

#include "raylib.h" // Vector2

#include "GameCamera.h" // WASD update + screen->world target
#include "Shortcuts.h"  // bound-key actions, polled per frame

// M2 Goal 6: input handling framework. Single per-frame polling point for
// everything M2 gathered piecemeal: WASD camera pan, mouse click edges,
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
    void Snapshot(Vector2 mouseScreenPos, bool leftPressed, bool rightPressed);

    Vector2 MouseScreen() const;
    bool LeftPressed() const;
    bool RightPressed() const;

    // Snapshot mouse position under the given camera view.
    Vector2 MouseWorld(const GameCamera &camera) const;

private:
    Vector2 mouseScreen_ = {};
    bool leftPressed_ = false;
    bool rightPressed_ = false;
};
