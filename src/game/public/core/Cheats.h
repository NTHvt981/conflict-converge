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

class CheatWidgetBase
{
public:
	CheatWidgetBase(const char *name, const char *file);
	CheatWidgetBase(const CheatWidgetBase &) = delete;
	CheatWidgetBase &operator=(const CheatWidgetBase &) = delete;
	virtual ~CheatWidgetBase();
	const char *Name() const;
	const char *File() const;
	virtual void DrawImGui() = 0;

private:
	std::string name_;
	std::string file_;
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

template <class Tag>
void DrawCheatWidget(const char *name, typename CheatValueType<Tag>::type &value);
template <> void DrawCheatWidget<TOGGLE>(const char *name, bool &value);
template <> void DrawCheatWidget<INT>(const char *name, int &value);
template <> void DrawCheatWidget<FLOAT>(const char *name, float &value);
template <> void DrawCheatWidget<BUTTON>(const char *name, bool &value);

template <class Tag> class CheatWidget : public CheatWidgetBase
{
public:
	using ValueT = typename CheatValueType<Tag>::type;
	CheatWidget(const char *name, const char *file, ValueT defaultValue)
		: CheatWidgetBase(name, file)
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
	void DrawImGui() override
	{
		DrawCheatWidget<Tag>(Name(), value_);
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
