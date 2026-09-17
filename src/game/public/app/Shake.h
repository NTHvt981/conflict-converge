#pragma once

// Screen shake (trauma pattern): impacts raise a 0..1 trauma value that
// bleeds off per frame; the rendered offset scales with trauma squared so
// small hits barely register while overlapping explosions compose (add and
// clamp). Pure math only — camera.view itself is never touched; Game renders
// through a shaken copy at BeginMode2D, so Pan/ClampToMap keep reasoning
// about the real, unshaken camera across frames.

constexpr float kShakeDecayRate = 1.5f; // trauma per second (full hit clears in ~0.7s)
constexpr float kShakeMaxPixels = 12.0f; // offset at trauma == 1
constexpr float kShakeHitTrauma = 0.2f;  // routine hit (sub-pixel: felt, not seen)
constexpr float kShakeDeathTrauma = 0.5f; // unit wipe (~3px jolt)

inline float AddShakeTrauma(float trauma, float amount)
{
    const float raised = trauma + amount;
    return raised >= 1.0f ? 1.0f : raised;
}

inline float DecayShakeTrauma(float trauma, float dt, float decayRate = kShakeDecayRate)
{
    const float aged = trauma - decayRate * dt;
    return aged <= 0.0f ? 0.0f : aged;
}

inline float ShakeMagnitude(float trauma, float maxPixels = kShakeMaxPixels)
{
    return trauma * trauma * maxPixels;
}
