#pragma once

#include <vector>

#include "raylib.h"
#include "core/Subsystem.h"

// Pure logic, headless-testable; Game owns draw calls and triggers.

enum class PingKind
{
    UnderAttack,
    UnitLost,
    Count
};

struct Ping
{
    Vector2 worldPos = {};
    PingKind kind = PingKind::UnderAttack;
    float age = 0.0f; // seconds since raised
};

class Pings : public Subsystem
{
public:
    Pings();
    void ResetForMatch() override;

    // Raise a ping stamped with live time; swallowed within the same kind's
    // retrigger floor (combat spam guard).
    void Raise(Vector2 worldPos, PingKind kind);
    // Explicit-clock variant.
    void RaiseAt(Vector2 worldPos, PingKind kind, double nowSeconds);
    // No-op on dt <= 0.
    void Update(float dt);
    const std::vector<Ping> &Active() const;
    // Most recent ping position for camera jump. False when none active.
    bool Latest(Vector2 &outPos) const;

private:
    std::vector<Ping> pings_;
    double lastRaise_[static_cast<int>(PingKind::Count)];
};
