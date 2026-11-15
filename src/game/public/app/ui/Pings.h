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

	// Live-time stamp; within a kind's retrigger floor the raise is dropped.
	void Raise(Vector2 worldPos, PingKind kind);
	void RaiseAt(Vector2 worldPos, PingKind kind, double nowSeconds);
	// No-op on dt <= 0.
	void Update(float dt);
	const std::vector<Ping> &Active() const;
	// Latest ping position for a camera jump; false when none is active.
	bool Latest(Vector2 &outPos) const;

private:
	std::vector<Ping> pings_;
	double lastRaise_[static_cast<int>(PingKind::Count)];
};
