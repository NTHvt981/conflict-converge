#include "app/ui/RmlUiWidgetAdapter.h"

#include <exception>
#include <string>

#include <RmlUi/Core.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>

#include "app/ui/UiWidget.h"

namespace {

float ReadRange(Rml::Element *element)
{
	if (auto *control = dynamic_cast<Rml::ElementFormControl *>(element))
	{
		try
		{
			return std::stof(control->GetValue().c_str());
		}
		catch (const std::exception &)
		{
		}
	}
	return 0.0f;
}

UiEvent Translate(const Rml::Event &event, Rml::Element *target)
{
	UiEvent out;
	if (event.GetType() != "change")
	{
		return out;
	}

	auto *control = dynamic_cast<Rml::ElementFormControl *>(target);
	if (control != nullptr && control->GetAttribute("type", Rml::String()) == "range")
	{
		out.kind = UiEventKind::Value;
		out.value = ReadRange(target);
		return out;
	}

	out.kind = UiEventKind::Toggle;
	out.checked = target->HasAttribute("checked");
	return out;
}

} // namespace

bool DispatchUiEvent(UiWidget *root, Rml::Event &event, Rml::Element *target)
{
	if (root == nullptr || target == nullptr)
	{
		return false;
	}

	UiWidget *widget = root->Find(target->GetId());
	if (widget == nullptr || !widget->HandleEvent(Translate(event, target)))
	{
		return false;
	}

	event.StopPropagation();
	return true;
}
