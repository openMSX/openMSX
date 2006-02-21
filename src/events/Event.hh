// $Id$

#ifndef EVENT_HH
#define EVENT_HH

namespace openmsx {

enum EventType
{
	OPENMSX_KEY_UP_EVENT,
	OPENMSX_KEY_DOWN_EVENT,
	OPENMSX_MOUSE_MOTION_EVENT,
	OPENMSX_MOUSE_BUTTON_UP_EVENT,
	OPENMSX_MOUSE_BUTTON_DOWN_EVENT,
	OPENMSX_JOY_AXIS_MOTION_EVENT,
	OPENMSX_JOY_BUTTON_UP_EVENT,
	OPENMSX_JOY_BUTTON_DOWN_EVENT,
	OPENMSX_FOCUS_EVENT,
	OPENMSX_RESIZE_EVENT,
	OPENMSX_QUIT_EVENT,
	OPENMSX_FINISH_FRAME_EVENT,
	OPENMSX_LED_EVENT,
	OPENMSX_BREAK_EVENT,
	OPENMSX_SWITCH_RENDERER_EVENT,
	/** Console events, allow the keyboard[joystick] to avoid
            having `hanging' keys */
	OPENMSX_CONSOLE_ON_EVENT,
	OPENMSX_CONSOLE_OFF_EVENT,

	/** Delayed repaint */
	OPENMSX_DELAYED_REPAINT_EVENT
};

class Event
{
public:
	EventType getType() const { return type; }
	virtual ~Event() {}

protected:
	explicit Event(EventType type_) : type(type_) {}

private:
	EventType type;
};

// implementation for events that don't need additional data
template<EventType T>
class SimpleEvent : public Event
{
public:
	SimpleEvent() : Event(T) {}
};

} // namespace openmsx

#endif
