
#include "app/ui/Vfx.h"

#include <cmath>
#include <cstdlib>

void Vfx::SpawnBurst(Vector2 center, Color color, int count, float speed, float life)
{
	for (int i = 0; i < count && particles_.size() < kMaxParticles; ++i)
	{
		const float angle =
			static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 2.0f * 3.14159265f;
		const float magnitude = speed * (0.4f + 0.6f * static_cast<float>(rand()) /
												   static_cast<float>(RAND_MAX));
		Particle p;
		p.pos = center;
		p.vel = { std::cos(angle) * magnitude, std::sin(angle) * magnitude };
		p.life = life;
		p.maxLife = life > 0.0f ? life : 1.0f;
		p.size = 4.0f + 4.0f * static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
		p.color = color;
		particles_.push_back(p);
	}
}

void Vfx::SpawnDamageNumber(Vector2 pos, float value)
{
	if (numbers_.size() >= kMaxDamageNumbers)
	{
		return;
	}
	DamageNumber n;
	n.pos = pos;
	n.value = value;
	n.life = kDamageNumberLife;
	n.maxLife = kDamageNumberLife;
	numbers_.push_back(n);
}

void Vfx::Update(float dt)
{
	for (std::size_t i = 0; i < particles_.size();)
	{
		Particle &p = particles_[i];
		p.life -= dt;
		if (p.life <= 0.0f)
		{
			p = particles_.back();
			particles_.pop_back();
			continue;
		}
		p.pos.x += p.vel.x * dt;
		p.pos.y += p.vel.y * dt;
		++i;
	}
	for (std::size_t i = 0; i < numbers_.size();)
	{
		DamageNumber &n = numbers_[i];
		n.life -= dt;
		if (n.life <= 0.0f)
		{
			n = numbers_.back();
			numbers_.pop_back();
			continue;
		}
		n.pos.y -= kDamageNumberRiseSpeed * dt;
		++i;
	}
}

void Vfx::DrawWorld() const
{
	for (const Particle &p : particles_)
	{
		const float alpha = p.life / p.maxLife;
		DrawCircleV(p.pos, p.size, Fade(p.color, alpha > 1.0f ? 1.0f : alpha));
	}
	for (const DamageNumber &n : numbers_)
	{
		const float alpha = n.life / n.maxLife;
		const char *text = TextFormat("-%.0f", n.value);
		const int x = static_cast<int>(n.pos.x) - MeasureText(text, 32) / 2;
		DrawText(text, x, static_cast<int>(n.pos.y), 32, Fade(RED, alpha > 1.0f ? 1.0f : alpha));
	}
}

void Vfx::Clear()
{
	particles_.clear();
	numbers_.clear();
}

void Vfx::ResetForMatch()
{
	Clear();
}

std::size_t Vfx::ParticleCount() const
{
	return particles_.size();
}

std::size_t Vfx::DamageNumberCount() const
{
	return numbers_.size();
}
