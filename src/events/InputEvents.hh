// $Id$

#ifndef INPUTEVENTS_HH
#define INPUTEVENTS_HH

#include "openmsx.hh"
#include "Event.hh"
#include "Keys.hh"

namespace openmsx {

/**
 * Base class for user input events.
 * TODO: Does not do anything special yet.
 *       I expect it will gain functionality in the future, but if it doesn't,
 *       maybe we shouldn't make it a separate class.
 */
class UserInputEvent : public Event
{
protected:
	explicit UserInputEvent(EventType type)
		: Event(type) {}
};

class HostKeyEvent : public Event
{
public:
	Keys::KeyCode getKeyCode() const { return keyCode; }
	word getUnicode() const { return unicode; }

protected:
	HostKeyEvent(EventType type, Keys::KeyCode keyCode_, word unicode_)
		: Event(type), keyCode(keyCode_), unicode(unicode_) {}

private:
	Keys::KeyCode keyCode;
	word unicode;
};

class HostKeyUpEvent : public HostKeyEvent
{
public:
	HostKeyUpEvent(Keys::KeyCode keyCode, word unicode)
		: HostKeyEvent(OPENMSX_HOST_KEY_UP_EVENT, keyCode, unicode) {}
};

class HostKeyDownEvent : public HostKeyEvent
{
public:
	HostKeyDownEvent(Keys::KeyCode keyCode, word unicode)
		: HostKeyEvent(OPENMSX_HOST_KEY_DOWN_EVENT, keyCode, unicode) {}
};

class EmuKeyEvent : public UserInputEvent
{
public:
	Keys::KeyCode getKeyCode() const { return keyCode; }
	word getUnicode() const { return unicode; }

protected:
	EmuKeyEvent(EventType type, Keys::KeyCode keyCode_, word unicode_)
		: UserInputEvent(type), keyCode(keyCode_), unicode(unicode_) {}

private:
	Keys::KeyCode keyCode;
	word unicode;
};

class EmuKeyUpEvent : public EmuKeyEvent
{
public:
	EmuKeyUpEvent(Keys::KeyCode keyCode, word unicode)
		: EmuKeyEvent(OPENMSX_EMU_KEY_UP_EVENT, keyCode, unicode) {}
};

class EmuKeyDownEvent : public EmuKeyEvent
{
public:
	EmuKeyDownEvent(Keys::KeyCode keyCode, word unicode)
		: EmuKeyEvent(OPENMSX_EMU_KEY_DOWN_EVENT, keyCode, unicode) {}
};


class MouseButtonEvent : public UserInputEvent
{
public:
	enum Button {
		LEFT, RIGHT, MIDDLE, OTHER
	};
	Button getButton() const { return button; }

protected:
	MouseButtonEvent(EventType type, Button button_)
		: UserInputEvent(type), button(button_) {}

private:
	Button button;
};

class MouseButtonUpEvent : public MouseButtonEvent
{
public:
	explicit MouseButtonUpEvent(MouseButtonEvent::Button button)
		: MouseButtonEvent(OPENMSX_MOUSE_BUTTON_UP_EVENT, button) {}
};

class MouseButtonDownEvent : public MouseButtonEvent
{
public:
	explicit MouseButtonDownEvent(MouseButtonEvent::Button button)
		: MouseButtonEvent(OPENMSX_MOUSE_BUTTON_DOWN_EVENT, button) {}
};

class MouseMotionEvent : public UserInputEvent
{
public:
	MouseMotionEvent(int xrel_, int yrel_)
		: UserInputEvent(OPENMSX_MOUSE_MOTION_EVENT)
		, xrel(xrel_), yrel(yrel_) {}

	int getX() const { return xrel; }
	int getY() const { return yrel; }

private:
	int xrel;
	int yrel;
};


class JoystickEvent : public UserInputEvent
{
public:
	unsigned getJoystick() const { return joystick; }

protected:
	JoystickEvent(EventType type, unsigned joystick_)
		: UserInputEvent(type), joystick(joystick_) {}

private:
	unsigned joystick;
};

class JoystickButtonEvent : public JoystickEvent
{
public:
	unsigned getButton() const { return button; }

protected:
	JoystickButtonEvent(EventType type, unsigned joystick, unsigned button_)
		: JoystickEvent(type, joystick), button(button_) {}

private:
	unsigned button;
};

class JoystickButtonUpEvent : public JoystickButtonEvent
{
public:
	JoystickButtonUpEvent(unsigned joystick, unsigned button)
		: JoystickButtonEvent(OPENMSX_JOY_BUTTON_UP_EVENT, joystick, button) {}
};

class JoystickButtonDownEvent : public JoystickButtonEvent
{
public:
	JoystickButtonDownEvent(unsigned joystick, unsigned button)
		: JoystickButtonEvent(OPENMSX_JOY_BUTTON_DOWN_EVENT, joystick, button) {}
};

class JoystickAxisMotionEvent : public JoystickEvent
{
public:
	enum Axis {
		X_AXIS, Y_AXIS, OTHER
	};
	JoystickAxisMotionEvent(unsigned joystick, Axis axis_, short value_)
		: JoystickEvent(OPENMSX_JOY_AXIS_MOTION_EVENT, joystick),
		  axis(axis_), value(value_) {}

	Axis getAxis() const { return axis; }
	short getValue() const { return value; }

private:
	Axis axis;
	short value;
};

/**
 * Used for console on/off events.
 * These events tell a lower layer (the MSX keyboard) that key down/up events
 * will be (on) or have been (off) withheld. With that information, the layer
 * can take measures against hanging keys.
 * TODO: Should this be modeled as an event, or as an additional call on
 *       the UserInputEventHandler interface?
 */
class ConsoleEvent : public UserInputEvent
{
public:
	explicit ConsoleEvent(EventType type) :
		UserInputEvent(type) {}
};

// Note: The following events are sent by the windowing system, but they
//       shouldn't be processed by the same listener stack as for example
//       keyboard events, so we won't let them inherit from UserInputEvent.
// TODO: Move them to a different file?

class FocusEvent : public Event
{
public:
	explicit FocusEvent(bool gain_)
		: Event(OPENMSX_FOCUS_EVENT), gain(gain_) {}

	bool getGain() const { return gain; }

private:
	bool gain;
};

class ResizeEvent : public Event
{
public:
	ResizeEvent(unsigned x_, unsigned y_)
		: Event(OPENMSX_RESIZE_EVENT), x(x_), y(y_) {}

	unsigned getX() const { return x; }
	unsigned getY() const { return y; }

private:
	unsigned x;
	unsigned y;
};

class QuitEvent : public Event
{
public:
	QuitEvent() : Event(OPENMSX_QUIT_EVENT) {}
};

} // namespace openmsx

#endif
