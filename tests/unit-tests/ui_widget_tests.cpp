#include "test_harness.h"

#include "app/ui/UiWidget.h"

#include <string_view>

void RunUiWidgetTests()
{
	UiWidget root("root");
	int presses = 0;
	UiButton *button = root.Add<UiButton>("btn");
	button->SetHandler([&presses] { ++presses; });

	CC_CHECK(button->Id() == "btn");
	CC_CHECK(root.Find("btn") == button);
	CC_CHECK(root.Find("missing") == nullptr);

	CC_CHECK(button->HandleEvent({ UiEventKind::Press, false, 0.0f }));
	CC_CHECK(presses == 1);
	CC_CHECK(!button->HandleEvent({ UiEventKind::Toggle, false, 0.0f }));
	CC_CHECK(presses == 1);

	UiWidget container("container");
	int outer = 0;
	int inner = 0;
	container.Add<UiButton>("outer")->SetHandler([&outer] { ++outer; });
	UiWidget *nested = container.Add<UiWidget>("nested");
	UiButton *innerButton = nested->Add<UiButton>("inner-btn");
	innerButton->SetHandler([&inner] { ++inner; });

	CC_CHECK(container.HandleEvent({ UiEventKind::Press, false, 0.0f }));
	CC_CHECK(outer == 1);
	CC_CHECK(inner == 0);
	CC_CHECK(innerButton->HandleEvent({ UiEventKind::Press, false, 0.0f }));
	CC_CHECK(inner == 1);

	bool checked = false;
	int toggles = 0;
	UiToggle toggle("toggle", [&](bool value) {
		checked = value;
		++toggles;
	});
	CC_CHECK(toggle.HandleEvent({ UiEventKind::Toggle, true, 0.0f }));
	CC_CHECK(checked);
	CC_CHECK(toggles == 1);
	CC_CHECK(toggle.HandleEvent({ UiEventKind::Toggle, false, 0.0f }));
	CC_CHECK(!checked);
	CC_CHECK(toggles == 2);

	float value = 0.0f;
	UiSlider slider("slider", [&](float next) { value = next; }, 100.0f, 800.0f);
	CC_CHECK(slider.HandleEvent({ UiEventKind::Value, false, 42.0f }));
	CC_CHECK(CcNear(value, 100.0f));
	CC_CHECK(slider.HandleEvent({ UiEventKind::Value, false, 2000.0f }));
	CC_CHECK(CcNear(value, 800.0f));
	CC_CHECK(slider.HandleEvent({ UiEventKind::Value, false, 400.0f }));
	CC_CHECK(CcNear(value, 400.0f));

	UiButton idle("idle");
	CC_CHECK(!idle.HandleEvent({ UiEventKind::Press, false, 0.0f }));
	UiWidget none("none");
	CC_CHECK(!none.HandleEvent({ UiEventKind::Press, false, 0.0f }));

	UiList list("list");
	int row = -1;
	list.Add<UiButton>("row-0", [&row] { row = 0; });
	list.Add<UiButton>("row-1", [&row] { row = 1; });
	CC_CHECK(list.Find("row-1") != nullptr);
	CC_CHECK(list.HandleEvent({ UiEventKind::Press, false, 0.0f }));
	CC_CHECK(row == 0);
	CC_CHECK(list.Find("row-1")->HandleEvent({ UiEventKind::Press, false, 0.0f }));
	CC_CHECK(row == 1);
	list.Clear();
	CC_CHECK(list.Find("row-0") == nullptr);
	CC_CHECK(!list.HandleEvent({ UiEventKind::Press, false, 0.0f }));
}
