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
             IsMouseButtonPressed(MOUSE_BUTTON_RIGHT));
}

void InputManager::Snapshot(Vector2 mouseScreenPos, bool leftPressed, bool rightPressed)
{
    mouseScreen_ = mouseScreenPos;
    leftPressed_ = leftPressed;
    rightPressed_ = rightPressed;
}

Vector2 InputManager::MouseScreen() const
{
    return mouseScreen_;
}

bool InputManager::LeftPressed() const
{
    return leftPressed_;
}

bool InputManager::RightPressed() const
{
    return rightPressed_;
}

Vector2 InputManager::MouseWorld(const GameCamera &camera) const
{
    return camera.ScreenToWorld(mouseScreen_);
}
