#include "app/ui/UiWidget.h"

#include <utility>

UiWidget::UiWidget(std::string_view id)
	: id_(id)
{
}

bool UiWidget::HandleEvent(const UiEvent &event)
{
	if (OnEvent(event))
	{
		return true;
	}

	for (const std::unique_ptr<UiWidget> &child : children_)
	{
		if (child->HandleEvent(event))
		{
			return true;
		}
	}
	return false;
}

UiWidget *UiWidget::Find(std::string_view id)
{
	if (id_ == id)
	{
		return this;
	}

	for (const std::unique_ptr<UiWidget> &child : children_)
	{
		if (UiWidget *found = child->Find(id))
		{
			return found;
		}
	}
	return nullptr;
}

bool UiWidget::OnEvent(const UiEvent &)
{
	return false;
}

UiButton::UiButton(std::string_view id, Handler handler)
	: UiWidget(id)
	, handler_(std::move(handler))
{
}

void UiButton::SetHandler(Handler handler)
{
	handler_ = std::move(handler);
}

bool UiButton::OnEvent(const UiEvent &event)
{
	if (event.kind != UiEventKind::Press || !handler_)
	{
		return false;
	}

	handler_();
	return true;
}

UiToggle::UiToggle(std::string_view id, Setter set)
	: UiWidget(id)
	, set_(std::move(set))
{
}

bool UiToggle::OnEvent(const UiEvent &event)
{
	if (event.kind != UiEventKind::Toggle || !set_)
	{
		return false;
	}

	set_(event.checked);
	return true;
}

UiSlider::UiSlider(std::string_view id, Setter set, float low, float high)
	: UiWidget(id)
	, set_(std::move(set))
	, low_(low)
	, high_(high)
{
}

bool UiSlider::OnEvent(const UiEvent &event)
{
	if (event.kind != UiEventKind::Value || !set_)
	{
		return false;
	}

	const float clamped = event.value < low_ ? low_ : (event.value > high_ ? high_ : event.value);
	set_(clamped);
	return true;
}
