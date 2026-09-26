#include "core/Cheats.h"

#if defined(CC_DEBUG)

namespace cc::cheat
{

CheatWidgetBase::CheatWidgetBase(const char *name, const char *file, CheatWidgetKind kind)
    : name_(name ? name : "")
    , kind_(kind)
{
    std::string full = file ? file : "";
    std::size_t pos = full.find_last_of("/\\");
    if (pos == std::string::npos)
    {
        file_ = full;
    }
    else
    {
        file_ = full.substr(pos + 1);
    }
}

CheatWidgetBase::~CheatWidgetBase() = default;

const char *CheatWidgetBase::Name() const
{
    return name_.c_str();
}

const char *CheatWidgetBase::File() const
{
    return file_.c_str();
}

CheatWidgetKind CheatWidgetBase::Kind() const
{
    return kind_;
}

CheatRegistry &CheatRegistry::Instance()
{
    static CheatRegistry instance;
    return instance;
}

void CheatRegistry::Add(CheatWidgetBase *widget)
{
    widgets_.push_back(widget);
}

const std::vector<CheatWidgetBase *> &CheatRegistry::Widgets() const
{
    return widgets_;
}

} // namespace cc::cheat

#endif
