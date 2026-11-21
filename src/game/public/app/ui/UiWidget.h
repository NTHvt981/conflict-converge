#pragma once

#include <functional>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

enum class UiEventKind
{
	Press,
	Toggle,
	Value,
	Count
};

struct UiEvent
{
	UiEventKind kind = UiEventKind::Press;
	bool checked = false;
	float value = 0.0f;
};

class UiWidget
{
public:
	explicit UiWidget(std::string_view id);

	virtual ~UiWidget() = default;
	UiWidget(const UiWidget &) = delete;
	UiWidget &operator=(const UiWidget &) = delete;

	std::string_view Id() const { return id_; }

	bool HandleEvent(const UiEvent &event);
	UiWidget *Find(std::string_view id);

	template <typename T, typename... Args>
	T *Add(Args &&...args)
	{
		std::unique_ptr<T> child = std::make_unique<T>(std::forward<Args>(args)...);
		T *raw = child.get();
		children_.push_back(std::move(child));
		return raw;
	}

protected:
	virtual bool OnEvent(const UiEvent &event);
	void ClearChildren();

private:
	std::string_view id_;
	std::vector<std::unique_ptr<UiWidget>> children_;
};

class UiButton : public UiWidget
{
public:
	using Handler = std::function<void()>;

	explicit UiButton(std::string_view id, Handler handler = {});

	void SetHandler(Handler handler);

protected:
	bool OnEvent(const UiEvent &event) override;

private:
	Handler handler_;
};

class UiToggle : public UiWidget
{
public:
	using Setter = std::function<void(bool)>;

	UiToggle(std::string_view id, Setter set);

protected:
	bool OnEvent(const UiEvent &event) override;

private:
	Setter set_;
};

class UiSlider : public UiWidget
{
public:
	using Setter = std::function<void(float)>;

	UiSlider(std::string_view id, Setter set, float low, float high);

protected:
	bool OnEvent(const UiEvent &event) override;

private:
	Setter set_;
	float low_;
	float high_;
};

class UiList : public UiWidget
{
public:
	explicit UiList(std::string_view id);

	void Clear();
};
