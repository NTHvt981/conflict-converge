#include "core/Event.h"

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
    const std::vector<Handler> handlers = it->second;
    for (const Handler &handler : handlers)
    {
        handler(event);
    }
}

void EventDispatcher::Clear()
{
    listeners_.clear();
}
