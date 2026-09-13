#include "InputManager.h"

void InputManager::Update(GameCamera &camera, float cameraSpeedPixelsPerSec, float dtSeconds)
{
    camera.UpdateWASD(cameraSpeedPixelsPerSec, dtSeconds);
    shortcuts.PollAndFire();
    PollLive();
}

void InputManager::PollLive()
{
    Snapshot(GetMousePosition(), IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
             IsMouseButtonPressed(MOUSE_BUTTON_RIGHT), GetMouseWheelMove(),
             IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT),
             IsMouseButtonDown(MOUSE_BUTTON_LEFT));
}

void InputManager::Snapshot(Vector2 mouseScreenPos, bool leftPressed, bool rightPressed, float wheelDelta,
                            bool shiftDown, bool leftDown)
{
    mouseScreen_ = mouseScreenPos;
    leftPressed_ = leftPressed;
    rightPressed_ = rightPressed;
    wheelDelta_ = wheelDelta;
    shiftDown_ = shiftDown;
    leftDown_ = leftDown;
}

Vector2 InputManager::MouseScreen() const
{
    return mouseScreen_;
}

bool InputManager::LeftPressed() const
{
    return leftPressed_;
}

bool InputManager::LeftDown() const
{
    return leftDown_;
}

bool InputManager::RightPressed() const
{
    return rightPressed_;
}

float InputManager::WheelDelta() const
{
    return wheelDelta_;
}

bool InputManager::ShiftDown() const
{
    return shiftDown_;
}

Vector2 InputManager::MouseWorld(const GameCamera &camera) const
{
    return camera.ScreenToWorld(mouseScreen_);
}
