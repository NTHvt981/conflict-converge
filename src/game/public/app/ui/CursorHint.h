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

// Cursor-following hint for RmlUi screens: one hint element per document,
// an entry table of element id -> hint text owned by the screen, and a
// per-frame Update that shows the hovered entry's text next to the cursor.
// The screen owns policy (text, gate); this owns mechanics (match, place).
struct CursorHintEntry
{
	std::string_view id;
	std::string text; // empty = no hint for this element
};

class CursorHint
{
public:
	// Binds one document and its hint element. The entry vector must
	// outlive the binding (the screen keeps it as a member); SetText
	// mutates its text members afterwards.
	void Bind(Rml::ElementDocument *doc, std::string_view hintId,
			  std::vector<CursorHintEntry> &entries);
	// Screen-owned policy: set or clear one entry's text by element id.
	void SetText(std::string_view id, std::string text);
	// Per frame: gate is the screen's own show condition (screen
	// visibility, modals). Within it the hint shows when the cursor's
	// ancestor chain reaches an entry id with non-empty text, anchored
	// just below-right of the cursor and clamped inside the window.
	void Update(Rml::Context *context, bool gate);

private:
	Rml::ElementDocument *doc_ = nullptr;
	Rml::Element *hint_ = nullptr;
	std::vector<CursorHintEntry> *entries_ = nullptr;
	std::string shownText_;
};
