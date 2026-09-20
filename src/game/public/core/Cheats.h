#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace cc::cheat
{

struct TOGGLE {};
struct INT {};
struct FLOAT {};
struct BUTTON {};

template <class Tag> struct CheatValueType;
template <> struct CheatValueType<TOGGLE> { using type = bool; };
template <> struct CheatValueType<INT> { using type = int; };
template <> struct CheatValueType<FLOAT> { using type = float; };
template <> struct CheatValueType<BUTTON> { using type = bool; };

enum class CheatWidgetKind { Toggle, Int, Float, Button };

template <class Tag> constexpr CheatWidgetKind CheatKindOf();
template <> constexpr CheatWidgetKind CheatKindOf<TOGGLE>() { return CheatWidgetKind::Toggle; }
template <> constexpr CheatWidgetKind CheatKindOf<INT>() { return CheatWidgetKind::Int; }
template <> constexpr CheatWidgetKind CheatKindOf<FLOAT>() { return CheatWidgetKind::Float; }
template <> constexpr CheatWidgetKind CheatKindOf<BUTTON>() { return CheatWidgetKind::Button; }

class CheatWidgetBase
{
public:
    CheatWidgetBase(const char *name, const char *file, CheatWidgetKind kind);
    CheatWidgetBase(const CheatWidgetBase &) = delete;
    CheatWidgetBase &operator=(const CheatWidgetBase &) = delete;
    virtual ~CheatWidgetBase();
    const char *Name() const;
    const char *File() const;
    CheatWidgetKind Kind() const;

private:
    std::string name_;
    std::string file_;
    CheatWidgetKind kind_;
};

class CheatRegistry
{
public:
    static CheatRegistry &Instance();
    void Add(CheatWidgetBase *widget);
    const std::vector<CheatWidgetBase *> &Widgets() const;

private:
    std::vector<CheatWidgetBase *> widgets_;
};

template <class Tag> class CheatWidget : public CheatWidgetBase
{
public:
    using ValueT = typename CheatValueType<Tag>::type;
    CheatWidget(const char *name, const char *file, ValueT defaultValue)
        : CheatWidgetBase(name, file, CheatKindOf<Tag>())
        , value_(defaultValue)
    {
        CheatRegistry::Instance().Add(this);
    }
    ValueT &value()
    {
        return value_;
    }
    const ValueT &value() const
    {
        return value_;
    }
    void Set(const ValueT &v)
    {
        value_ = v;
    }

private:
    ValueT value_;
};

} // namespace cc::cheat

#if defined(CC_DEBUG)
#define DEFINE_CHEAT_WIDGET(name, type, default_value) \
    static ::cc::cheat::CheatWidget<::cc::cheat::type> name(#name, __FILE__, default_value);
#define DEFINE_CHEAT_CODE(...) __VA_ARGS__
#else
#define DEFINE_CHEAT_WIDGET(name, type, default_value)
#define DEFINE_CHEAT_CODE(...)
#endif
