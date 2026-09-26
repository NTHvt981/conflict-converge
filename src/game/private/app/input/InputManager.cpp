#include "app/input/InputManager.h"

void InputManager::Update(GameCamera &camera, float cameraSpeedPixelsPerSec, float dtSeconds)
{
    camera.UpdateWASD(cameraSpeedPixelsPerSec, dtSeconds);
    camera.UpdateEdgePan(cameraSpeedPixelsPerSec, dtSeconds, GetScreenWidth(), GetScreenHeight());
    shortcuts.PollAndFire();
    PollLive();
}

void InputManager::PollLive()
{
    Snapshot(GetMousePosition(), IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
             IsMouseButtonPressed(MOUSE_BUTTON_RIGHT), GetMouseWheelMove(),
             IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT),
             IsMouseButtonDown(MOUSE_BUTTON_LEFT), IsMouseButtonDown(MOUSE_BUTTON_RIGHT),
             IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL), GetTime());
}

void InputManager::Snapshot(Vector2 mouseScreenPos, bool leftPressed, bool rightPressed, float wheelDelta,
                            bool shiftDown, bool leftDown, bool rightDown, bool ctrlDown,
                            double nowSeconds)
{
    if (!hasPrevMouse_)
    {
        prevMouseScreen_ = mouseScreenPos;
        hasPrevMouse_ = true;
    }
    mouseDelta_ = { mouseScreenPos.x - prevMouseScreen_.x,
                    mouseScreenPos.y - prevMouseScreen_.y };
    prevMouseScreen_ = mouseScreenPos;
    mouseScreen_ = mouseScreenPos;
    leftPressed_ = leftPressed;
    rightPressed_ = rightPressed;
    wheelDelta_ = wheelDelta;
    shiftDown_ = shiftDown;
    leftDown_ = leftDown;
    rightDown_ = rightDown;
    ctrlDown_ = ctrlDown;
    doubleClicked_ = false;
    if (leftPressed)
    {
        constexpr double kWindowSeconds = 0.35;
        constexpr float kTolerancePx = 8.0f;
        const float dx = mouseScreenPos.x - lastLeftClickPos_.x;
        const float dy = mouseScreenPos.y - lastLeftClickPos_.y;
        if (lastLeftClickTime_ >= 0.0 && nowSeconds - lastLeftClickTime_ <= kWindowSeconds &&
            dx * dx + dy * dy <= kTolerancePx * kTolerancePx)
        {
            doubleClicked_ = true;
            lastLeftClickTime_ = -1.0;
        }
        else
        {
            lastLeftClickTime_ = nowSeconds;
            lastLeftClickPos_ = mouseScreenPos;
        }
    }
}

Vector2 InputManager::MouseScreen() const
{
    return mouseScreen_;
}

Vector2 InputManager::MouseDeltaScreen() const
{
    return mouseDelta_;
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

bool InputManager::RightDown() const
{
    return rightDown_;
}

float InputManager::WheelDelta() const
{
    return wheelDelta_;
}

bool InputManager::ShiftDown() const
{
    return shiftDown_;
}

bool InputManager::CtrlDown() const
{
    return ctrlDown_;
}

bool InputManager::DoubleClicked() const
{
    return doubleClicked_;
}

Vector2 InputManager::MouseWorld(const GameCamera &camera) const
{
    return camera.ScreenToWorld(mouseScreen_);
}
