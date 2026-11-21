#pragma once

namespace Rml
{
class Element;
class Event;
}

class UiWidget;

// True when a widget consumed the event; propagation is already stopped.
bool DispatchUiEvent(UiWidget *root, Rml::Event &event, Rml::Element *target);
