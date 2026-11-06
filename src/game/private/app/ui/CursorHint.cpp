#include "app/ui/CursorHint.h"

#include <cstdio>

#include "raylib.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>

namespace {

constexpr int kCursorHintOffsetX = 14;
constexpr int kCursorHintOffsetY = 22;
constexpr int kCursorHintWidthReserve = 240;
constexpr int kCursorHintHeightReserve = 40;
constexpr int kCursorHintEdge = 4;

int ClampPx(int value, int lo, int hi)
{
	if (hi < lo)
	{
		hi = lo;
	}
	return value < lo ? lo : (value > hi ? hi : value);
}

void SetPositionPx(Rml::Element *hint, int cursorX, int cursorY)
{
	char text[32];
	snprintf(text, sizeof(text), "%dpx",
			 ClampPx(cursorX + kCursorHintOffsetX, kCursorHintEdge,
					 GetScreenWidth() - kCursorHintWidthReserve));
	hint->SetProperty("left", text);
	snprintf(text, sizeof(text), "%dpx",
			 ClampPx(cursorY + kCursorHintOffsetY, kCursorHintEdge,
					 GetScreenHeight() - kCursorHintHeightReserve));
	hint->SetProperty("top", text);
}

} // namespace

void CursorHint::Bind(Rml::ElementDocument *doc, std::string_view hintId,
					  std::vector<CursorHintEntry> &entries)
{
	doc_ = doc;
	entries_ = &entries;
	hint_ = doc != nullptr ? doc->GetElementById(std::string(hintId)) : nullptr;
	if (hint_ != nullptr)
	{
		hint_->SetInnerRML("");
		shownText_.clear();
	}
}

void CursorHint::SetText(std::string_view id, std::string text)
{
	if (entries_ == nullptr)
	{
		return;
	}
	for (CursorHintEntry &entry : *entries_)
	{
		if (entry.id == id)
		{
			entry.text = std::move(text);
			return;
		}
	}
}

void CursorHint::Update(Rml::Context *context, bool gate)
{
	if (hint_ == nullptr || entries_ == nullptr)
	{
		return;
	}
	std::string_view text;
	if (gate && context != nullptr)
	{
		for (Rml::Element *el = context->GetHoverElement(); el != nullptr;
			 el = el->GetParentNode())
		{
			for (const CursorHintEntry &entry : *entries_)
			{
				if (!entry.text.empty() && el->GetId() == Rml::String(entry.id))
				{
					text = entry.text;
					break;
				}
			}
			if (!text.empty() || el == doc_)
			{
				break;
			}
		}
	}
	if (text.empty())
	{
		hint_->SetProperty("display", Rml::String("none"));
		shownText_.clear();
		return;
	}
	if (shownText_ != text)
	{
		hint_->SetInnerRML(Rml::String(text));
		shownText_ = std::string(text);
	}
	hint_->SetProperty("display", Rml::String("block"));
	const Vector2 mouse = GetMousePosition();
	SetPositionPx(hint_, static_cast<int>(mouse.x), static_cast<int>(mouse.y));
}
