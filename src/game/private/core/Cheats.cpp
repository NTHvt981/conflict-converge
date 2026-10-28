#include "core/Cheats.h"

#if defined(CC_DEBUG)

#include "imgui.h"

namespace cc::cheat
{

CheatWidgetBase::CheatWidgetBase(const char *name, const char *file)
	: name_(name ? name : "")
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

template <> void DrawCheatWidget<TOGGLE>(const char *name, bool &value)
{
	bool checked = value;
	if (ImGui::Checkbox(name, &checked))
	{
		value = checked;
	}
}

template <> void DrawCheatWidget<INT>(const char *name, int &value)
{
	int edited = value;
	ImGui::InputInt(name, &edited);
	value = edited;
}

template <> void DrawCheatWidget<FLOAT>(const char *name, float &value)
{
	float edited = value;
	ImGui::DragFloat(name, &edited);
	value = edited;
}

template <> void DrawCheatWidget<BUTTON>(const char *name, bool &value)
{
	value = ImGui::Button(name);
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
