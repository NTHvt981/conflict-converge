#pragma once

#include <RmlUi/Core/SystemInterface.h>

class RmlRaylibSystemInterface : public Rml::SystemInterface {
public:
	double GetElapsedTime() override;
	bool LogMessage(Rml::Log::Type type, const Rml::String& message) override;
	void SetMouseCursor(const Rml::String& cursor_name) override;
	void SetClipboardText(const Rml::String& text) override;
	void GetClipboardText(Rml::String& text) override;
};
