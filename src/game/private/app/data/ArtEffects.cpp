
#include "app/data/Art.h"

#include <cmath>
#include <cstdlib>

void Particles::SpawnBurst(Vector2 center, Color color, int count, float speed, float life)
{
    for (int i = 0; i < count && live_.size() < kMax; ++i)
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
        p.size = 2.0f + 2.0f * static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        p.color = color;
        live_.push_back(p);
    }
}

void Particles::Update(float dt)
{
    for (std::size_t i = 0; i < live_.size();)
    {
        Particle &p = live_[i];
        p.life -= dt;
        if (p.life <= 0.0f)
        {
            p = live_.back();
            live_.pop_back();
            continue;
        }
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;
        ++i;
    }
}

void Particles::Draw() const
{
    for (const Particle &p : live_)
    {
        const float alpha = p.life / p.maxLife;
        DrawCircleV(p.pos, p.size, Fade(p.color, alpha > 1.0f ? 1.0f : alpha));
    }
}

void Particles::Clear()
{
    live_.clear();
}

std::size_t Particles::Count() const
{
    return live_.size();
}

void DamageNumbers::Spawn(Vector2 pos, float value)
{
    if (live_.size() >= kMax)
    {
        return;
    }
    DamageNumber n;
    n.pos = pos;
    n.value = value;
    n.life = kLife;
    n.maxLife = kLife;
    live_.push_back(n);
}

void DamageNumbers::Update(float dt)
{
    for (std::size_t i = 0; i < live_.size();)
    {
        DamageNumber &n = live_[i];
        n.life -= dt;
        if (n.life <= 0.0f)
        {
            n = live_.back();
            live_.pop_back();
            continue;
        }
        n.pos.y -= kRiseSpeed * dt;
        ++i;
    }
}

void DamageNumbers::Draw() const
{
    for (const DamageNumber &n : live_)
    {
        const float alpha = n.life / n.maxLife;
        const char *text = TextFormat("-%.0f", n.value);
        const int x = static_cast<int>(n.pos.x) - MeasureText(text, 16) / 2;
        DrawText(text, x, static_cast<int>(n.pos.y), 16, Fade(RED, alpha > 1.0f ? 1.0f : alpha));
    }
}

void DamageNumbers::Clear()
{
    live_.clear();
}

std::size_t DamageNumbers::Count() const
{
    return live_.size();
}
