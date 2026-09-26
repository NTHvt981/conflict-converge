#pragma once

// Screen shake (trauma pattern): pure math only; Game renders through a
// shaken camera copy, never mutating camera.view.

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
