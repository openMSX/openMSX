// $Id$

#ifndef INPUTEVENTS_HH
#define INPUTEVENTS_HH

#include "openmsx.hh"
#include "Event.hh"
#include "Keys.hh"

namespace openmsx {

class KeyEvent : public Event
{
public:
	Keys::KeyCode getKeyCode() const { return keyCode; }
	word getUnicode() const { return unicode; }
	
protected:
	KeyEvent(EventType type, Keys::KeyCode keyCode_, word unicode_) :
		Event(type), keyCode(keyCode_), unicode(unicode_) {}

private:
	Keys::KeyCode keyCode;
	word unicode;
};

class KeyUpEvent : public KeyEvent
{
public:
	KeyUpEvent(Keys::KeyCode keyCode, word unicode)
		: KeyEvent(KEY_UP_EVENT, keyCode, unicode) {}
};

class KeyDownEvent : public KeyEvent
{
public:
	KeyDownEvent(Keys::KeyCode keyCode, word unicode)
		: KeyEvent(KEY_DOWN_EVENT, keyCode, unicode) {}
};


class MouseButtonEvent : public Event
{
public:
	enum Button {
		LEFT, RIGHT, MIDDLE, OTHER
	};
	Button getButton() const { return button; }
	
protected:
	MouseButtonEvent(EventType type, Button button_)
		: Event(type), button(button_) {}

private:
	Button button;
};

class MouseButtonUpEvent : public MouseButtonEvent
{
public:
	MouseButtonUpEvent(MouseButtonEvent::Button button)
		: MouseButtonEvent(MOUSE_BUTTON_UP_EVENT, button) {}
};

class MouseButtonDownEvent : public MouseButtonEvent
{
public:
	MouseButtonDownEvent(MouseButtonEvent::Button button)
		: MouseButtonEvent(MOUSE_BUTTON_DOWN_EVENT, button) {}
};

class MouseMotionEvent : public Event
{
public:
	MouseMotionEvent(int xrel_, int yrel_)
		: Event(MOUSE_MOTION_EVENT), xrel(xrel_), yrel(yrel_) {}

	int getX() const { return xrel; }
	int getY() const { return yrel; }

private:
	int xrel;
	int yrel;
};


class JoystickEvent : public Event
{
public:
	unsigned getJoystick() const { return joystick; }

protected:
	JoystickEvent(EventType type, unsigned joystick_)
		: Event(type), joystick(joystick_) {}

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
		: JoystickButtonEvent(JOY_BUTTON_UP_EVENT, joystick, button) {}
};

class JoystickButtonDownEvent : public JoystickButtonEvent
{
public:
	JoystickButtonDownEvent(unsigned joystick, unsigned button)
		: JoystickButtonEvent(JOY_BUTTON_DOWN_EVENT, joystick, button) {}
};

class JoystickAxisMotionEvent : public JoystickEvent
{
public:
	enum Axis {
		X_AXIS, Y_AXIS, OTHER
	};
	JoystickAxisMotionEvent(unsigned joystick, Axis axis_, short value_)
		: JoystickEvent(JOY_AXIS_MOTION_EVENT, joystick),
		  axis(axis_), value(value_) {}

	Axis getAxis() const { return axis; }
	short getValue() const { return value; }

private:
	Axis axis;
	short value;
};

class FocusEvent : public Event
{
public:
	FocusEvent(bool gain_)
		: Event(FOCUS_EVENT), gain(gain_) {}

	bool getGain() const { return gain; }
	
private:
	bool gain;
};

class ResizeEvent : public Event
{
public:
	ResizeEvent(unsigned x_, unsigned y_)
		: Event(RESIZE_EVENT), x(x_), y(y_) {}

	unsigned getX() const { return x; }
	unsigned getY() const { return y; }

private:
	unsigned x;
	unsigned y;
};

class QuitEvent : public Event
{
public:
	QuitEvent() : Event(QUIT_EVENT) {}
};

} // namespace openmsx

#endif
