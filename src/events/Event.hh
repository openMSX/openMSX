// $Id$

#ifndef __EVENT_HH__
#define __EVENT_HH__

namespace openmsx {

enum EventType
{
	KEY_UP_EVENT,
	KEY_DOWN_EVENT,
	MOUSE_MOTION_EVENT,
	MOUSE_BUTTON_UP_EVENT,
	MOUSE_BUTTON_DOWN_EVENT,
	JOY_AXIS_MOTION_EVENT,
	JOY_BUTTON_UP_EVENT,
	JOY_BUTTON_DOWN_EVENT,
	FOCUS_EVENT,
	QUIT_EVENT,
	FINISH_FRAME_EVENT,
	LED_EVENT,
	/** Triggers a renderer switch sequence. */
	RENDERER_SWITCH_EVENT,
	/** Sent when video system has been switched,
	  * to tell VDPs they can create a new renderer now.
	  */
	RENDERER_SWITCH2_EVENT,
	BREAK_EVENT,
	/** Console events, allow the keyboard[joystick] to avoid
            having `hanging' keys */
	CONSOLE_ON_EVENT,
	CONSOLE_OFF_EVENT,

	/** Delayed repaint */
	DELAYED_REPAINT_EVENT,
};

class Event
{
public:
	EventType getType() const { return type; }
	virtual ~Event() {}

protected:
	Event(EventType type_) : type(type_) {}

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

#endif // __EVENTDISTRIBUTOR_HH__
