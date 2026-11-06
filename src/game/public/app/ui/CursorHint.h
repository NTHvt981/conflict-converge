#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace Rml
{
class Context;
class Element;
class ElementDocument;
}

// Cursor-following hint for RmlUi screens. The screen owns policy (the
// entry texts and the Update gate); this matches the hovered element and
// places the hint.
struct CursorHintEntry
{
	std::string_view id;
	std::string text;
};

class CursorHint
{
public:
	// entries must outlive the binding
	void Bind(Rml::ElementDocument *doc, std::string_view hintId,
			  std::vector<CursorHintEntry> &entries);
	void SetText(std::string_view id, std::string text);
	void Update(Rml::Context *context, bool gate);

private:
	Rml::ElementDocument *doc_ = nullptr;
	Rml::Element *hint_ = nullptr;
	std::vector<CursorHintEntry> *entries_ = nullptr;
	std::string shownText_;
};
