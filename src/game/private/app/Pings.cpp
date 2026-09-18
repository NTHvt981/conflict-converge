#include "Pings.h"

namespace
{

constexpr float kPingLifetimeSeconds = 5.0f;
constexpr double kPingRetriggerFloorSeconds = 2.0;
constexpr std::size_t kMaxPings = 32;

} // namespace

Pings::Pings()
{
    for (double &last : lastRaise_)
    {
        last = -1.0e9; // first raise of each kind always lands
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
        return; // same-kind spam guard (a unit under continuous fire)
    }
    lastRaise_[i] = nowSeconds;
    if (pings_.size() >= kMaxPings)
    {
        pings_.erase(pings_.begin()); // drop oldest, keep the list bounded
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
    // Order-preserving erase: RaiseAt evicts from the front (oldest) and
    // Latest reads the back (newest), so survivors must stay chronological.
    // Swap-and-pop would be O(1) per removal but breaks both assumptions;
    // at kMaxPings (32) the linear erase is negligible.
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
