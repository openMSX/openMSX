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
	QUIT_EVENT,
	FINISH_FRAME_EVENT,
};

class Event
{
public:
	EventType getType() const { return type; }

protected:
	Event(EventType type_) : type(type_) {}
	virtual ~Event() {}

private:
	EventType type;
};

} // namespace openmsx

#endif // __EVENTDISTRIBUTOR_HH__
