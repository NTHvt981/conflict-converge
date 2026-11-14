#pragma once

#include <cstddef>
#include <vector>

#include "raylib.h"
#include "core/Subsystem.h"

// World-space effect pools: particle bursts (impact sparks, muzzle flashes,
// death bursts) and floating damage numbers. Logic is device-independent
// (tested headless); DrawWorld needs a live window.

struct Particle
{
	Vector2 pos = {};
	Vector2 vel = {};
	float life = 0.0f;
	float maxLife = 1.0f;
	float size = 3.0f;
	Color color = WHITE;
};

struct DamageNumber
{
	Vector2 pos = {};
	float value = 0.0f;
	float life = 0.0f;
	float maxLife = 1.0f;
};

class Vfx : public Subsystem
{
public:
	static constexpr std::size_t kMaxParticles = 512;
	static constexpr std::size_t kMaxDamageNumbers = 256;
	static constexpr float kDamageNumberLife = 0.7f;
	static constexpr float kDamageNumberRiseSpeed = 80.0f; // pixels per second, upward

	void SpawnBurst(Vector2 center, Color color, int count, float speed, float life);
	void SpawnDamageNumber(Vector2 pos, float value);

	void Update(float dt);
	void DrawWorld() const; // world-space: call inside BeginMode2D/EndMode2D
	void Clear();
	void ResetForMatch() override;

	std::size_t ParticleCount() const;
	std::size_t DamageNumberCount() const;

private:
	std::vector<Particle> particles_;
	std::vector<DamageNumber> numbers_;
};
