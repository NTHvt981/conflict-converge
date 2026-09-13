#include "Event.h"

// M1 Goal 5: type-routed publish/subscribe dispatcher.

void EventDispatcher::Subscribe(EventType type, Handler handler)
{
    listeners_[type].push_back(std::move(handler));
}

void EventDispatcher::Dispatch(const Event &event) const
{
    auto it = listeners_.find(event.type);
    if (it == listeners_.end())
    {
        return;
    }
    for (const Handler &handler : it->second)
    {
        handler(event);
    }
}

void EventDispatcher::Clear()
{
    listeners_.clear();
}
