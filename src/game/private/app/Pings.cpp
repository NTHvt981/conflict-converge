#include "Pings.h"

namespace
{

constexpr float kPingLifetimeSeconds = 5.0f;
constexpr double kPingRetriggerFloorSeconds = 2.0;
constexpr std::size_t kMaxPings = 32;

}

Pings::Pings()
{
    ResetForMatch();
}

void Pings::ResetForMatch()
{
    pings_.clear();
    for (double &last : lastRaise_)
    {
        last = -1.0e9;
    }
}

void Pings::Raise(Vector2 worldPos, PingKind kind)
{
    RaiseAt(worldPos, kind, GetTime());
}

void Pings::RaiseAt(Vector2 worldPos, PingKind kind, double nowSeconds)
{
    const int i = static_cast<int>(kind);
    if (i < 0 || i >= static_cast<int>(PingKind::Count))
    {
        return;
    }
    if (nowSeconds - lastRaise_[i] < kPingRetriggerFloorSeconds)
    {
        return;
    }
    lastRaise_[i] = nowSeconds;
    if (pings_.size() >= kMaxPings)
    {
        pings_.erase(pings_.begin());
    }
    Ping ping;
    ping.worldPos = worldPos;
    ping.kind = kind;
    ping.age = 0.0f;
    pings_.push_back(ping);
}

void Pings::Update(float dt)
{
    if (dt <= 0.0f)
    {
        return;
    }
    for (std::size_t i = 0; i < pings_.size();)
    {
        pings_[i].age += dt;
        if (pings_[i].age > kPingLifetimeSeconds)
        {
            pings_.erase(pings_.begin() + i);
            continue;
        }
        ++i;
    }
}

const std::vector<Ping> &Pings::Active() const
{
    return pings_;
}

bool Pings::Latest(Vector2 &outPos) const
{
    if (pings_.empty())
    {
        return false;
    }
    outPos = pings_.back().worldPos;
    return true;
}
