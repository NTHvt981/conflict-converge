#pragma once

#include <vector>

#include "raylib.h"

// QoL attack/event pings: short-lived world-space markers ("your base is
// under attack") drawn as minimap blips, with camera-jump support. Pure
// logic (Raise/Update/expiry), headless-testable; Game.cpp owns the draw
// calls and the damage/spawn/death triggers.

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
    float age = 0.0f; // seconds since raised; blips fade/pulse by age
};

class Pings
{
public:
    Pings();

    // Raise a ping stamped with live time. Swallowed when the same kind
    // raised less than the retrigger floor ago (combat spam guard).
    void Raise(Vector2 worldPos, PingKind kind);
    // Same, with an explicit clock (tests drive this directly).
    void RaiseAt(Vector2 worldPos, PingKind kind, double nowSeconds);
    // Age out pings older than the lifetime. No-op on dt <= 0.
    void Update(float dt);
    const std::vector<Ping> &Active() const;
    // Most recent ping position for camera jump. False when none active.
    bool Latest(Vector2 &outPos) const;

private:
    std::vector<Ping> pings_;
    double lastRaise_[static_cast<int>(PingKind::Count)];
};
