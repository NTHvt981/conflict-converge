#pragma once

#include <functional>
#include <unordered_map>

// Keys are raylib key codes, actions nullary callbacks. Fire invokes one binding
// directly so routing is testable headless.

class ShortcutRegistry
{
public:
	using Action = std::function<void()>;

	void Bind(int raylibKey, Action action);
	// Bind a Shift+key chord; plain Bind never requires shift.
	void BindChord(int raylibKey, Action action);
	void Unbind(int raylibKey);
	bool Has(int raylibKey) const;

	bool Fire(int raylibKey) const;
	// Fires only when a shift-chord is bound.
	bool FireChord(int raylibKey) const;
	// Fire one key honoring PollAndFire's shift-chord priority.
	bool FireWithShift(int raylibKey, bool shift) const;

	void PollAndFire() const;

	// Disabled: every Fire* path is a no-op; gates shortcuts behind a modal.
	void SetEnabled(bool enabled) { enabled_ = enabled; }
	bool Enabled() const { return enabled_; }

	void Clear();

private:
	struct KeyBindings
	{
		Action plain;
		Action chord;
		bool hasPlain = false;
		bool hasChord = false;
	};
	std::unordered_map<int, KeyBindings> bindings_;
	bool enabled_ = true;
};
