#include "Event.h"

// Type-routed publish/subscribe dispatcher.

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
    // Snapshot: handlers may re-entrantly Subscribe/Clear, reallocating (or
    // emptying) the live vector mid-iteration. Re-entrant changes apply to
    // the next Dispatch, never the in-flight one.
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
