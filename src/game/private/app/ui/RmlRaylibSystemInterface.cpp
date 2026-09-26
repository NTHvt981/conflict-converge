#include "app/ui/RmlRaylibSystemInterface.h"

#include <raylib.h>

double RmlRaylibSystemInterface::GetElapsedTime()
{
	return GetTime();
}

bool RmlRaylibSystemInterface::LogMessage(Rml::Log::Type type, const Rml::String& message)
{
	switch (type)
	{
	case Rml::Log::LT_ERROR: TraceLog(LOG_ERROR, "RmlUi: %s", message.c_str()); break;
	case Rml::Log::LT_WARNING: TraceLog(LOG_WARNING, "RmlUi: %s", message.c_str()); break;
	default: TraceLog(LOG_INFO, "RmlUi: %s", message.c_str()); break;
	}
	return true;
}

void RmlRaylibSystemInterface::SetMouseCursor(const Rml::String& cursor_name)
{
	if (cursor_name.empty() || cursor_name == "arrow")
		::SetMouseCursor(MOUSE_CURSOR_DEFAULT);
	else if (cursor_name == "cross")
		::SetMouseCursor(MOUSE_CURSOR_CROSSHAIR);
	else if (cursor_name == "text")
		::SetMouseCursor(MOUSE_CURSOR_IBEAM);
	else if (cursor_name == "move")
		::SetMouseCursor(MOUSE_CURSOR_RESIZE_ALL);
	else if (cursor_name == "resize")
		::SetMouseCursor(MOUSE_CURSOR_RESIZE_NWSE);
	else if (cursor_name == "unavailable")
		::SetMouseCursor(MOUSE_CURSOR_NOT_ALLOWED);
	else if (cursor_name == "pointer")
		::SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
}

void RmlRaylibSystemInterface::SetClipboardText(const Rml::String& text)
{
	::SetClipboardText(text.c_str());
}

void RmlRaylibSystemInterface::GetClipboardText(Rml::String& text)
{
	const char* clipboard = ::GetClipboardText();
	text = clipboard ? Rml::String(clipboard) : Rml::String();
}
