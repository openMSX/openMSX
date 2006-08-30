// $Id$

#ifndef INPUTEVENTS_HH
#define INPUTEVENTS_HH

#include "openmsx.hh"
#include "Event.hh"
#include "Keys.hh"
#include <string>

namespace openmsx {

class InputEvent : public Event
{
public:
	virtual std::string toString() const = 0;
protected:
	virtual bool lessImpl(const Event& other) const;
	virtual bool lessImpl(const InputEvent& other) const = 0;
	explicit InputEvent(EventType type);
};


class KeyEvent : public InputEvent
{
public:
	Keys::KeyCode getKeyCode() const;
	word getUnicode() const;
	virtual std::string toString() const;

protected:
	KeyEvent(EventType type, Keys::KeyCode keyCode, word unicode);

private:
	virtual bool lessImpl(const InputEvent& other) const;
	Keys::KeyCode keyCode;
	word unicode;
};

class KeyUpEvent : public KeyEvent
{
public:
	KeyUpEvent(Keys::KeyCode keyCode);
	KeyUpEvent(Keys::KeyCode keyCode, word unicode);
};

class KeyDownEvent : public KeyEvent
{
public:
	KeyDownEvent(Keys::KeyCode keyCode);
	KeyDownEvent(Keys::KeyCode keyCode, word unicode);
};


class MouseButtonEvent : public InputEvent
{
public:
	static const unsigned LEFT      = 1;
	static const unsigned MIDDLE    = 2;
	static const unsigned RIGHT     = 3;
	static const unsigned WHEELUP   = 4;
	static const unsigned WHEELDOWN = 5;

	unsigned getButton() const;

protected:
	MouseButtonEvent(EventType type, unsigned button_);
	std::string toStringHelper() const;

private:
	virtual bool lessImpl(const InputEvent& other) const;
	unsigned button;
};

class MouseButtonUpEvent : public MouseButtonEvent
{
public:
	explicit MouseButtonUpEvent(unsigned button);
	virtual std::string toString() const;
};

class MouseButtonDownEvent : public MouseButtonEvent
{
public:
	explicit MouseButtonDownEvent(unsigned button);
	virtual std::string toString() const;
};

class MouseMotionEvent : public InputEvent
{
public:
	MouseMotionEvent(int xrel, int yrel);
	int getX() const;
	int getY() const;
	virtual std::string toString() const;

private:
	virtual bool lessImpl(const InputEvent& other) const;
	int xrel;
	int yrel;
};


class JoystickEvent : public InputEvent
{
public:
	unsigned getJoystick() const;

protected:
	JoystickEvent(EventType type, unsigned joystick);
	std::string toStringHelper() const;

private:
	virtual bool lessImpl(const InputEvent& other) const;
	virtual bool lessImpl(const JoystickEvent& other) const = 0;
	unsigned joystick;
};

class JoystickButtonEvent : public JoystickEvent
{
public:
	unsigned getButton() const;

protected:
	JoystickButtonEvent(EventType type, unsigned joystick, unsigned button);
	std::string toStringHelper() const;

private:
	virtual bool lessImpl(const JoystickEvent& other) const;
	unsigned button;
};

class JoystickButtonUpEvent : public JoystickButtonEvent
{
public:
	JoystickButtonUpEvent(unsigned joystick, unsigned button);
	virtual std::string toString() const;
};

class JoystickButtonDownEvent : public JoystickButtonEvent
{
public:
	JoystickButtonDownEvent(unsigned joystick, unsigned button);
	virtual std::string toString() const;
};

class JoystickAxisMotionEvent : public JoystickEvent
{
public:
	static const unsigned X_AXIS = 0;
	static const unsigned Y_AXIS = 1;

	JoystickAxisMotionEvent(unsigned joystick, unsigned axis, short value);
	unsigned getAxis() const;
	short getValue() const;
	virtual std::string toString() const;

private:
	virtual bool lessImpl(const JoystickEvent& other) const;
	unsigned axis;
	short value;
};


class FocusEvent : public InputEvent
{
public:
	explicit FocusEvent(bool gain);

	bool getGain() const;
	virtual std::string toString() const;

private:
	virtual bool lessImpl(const InputEvent& other) const;
	bool gain;
};


class ResizeEvent : public InputEvent
{
public:
	ResizeEvent(unsigned x, unsigned y);

	unsigned getX() const;
	unsigned getY() const;
	virtual std::string toString() const;

private:
	virtual bool lessImpl(const InputEvent& other) const;
	unsigned x;
	unsigned y;
};


class QuitEvent : public InputEvent
{
public:
	QuitEvent();
	virtual std::string toString() const;
private:
	virtual bool lessImpl(const InputEvent& other) const;
};


/**
 * Used for console on/off events.
 * These events tell a lower layer (the MSX keyboard) that key down/up events
 * will be (on) or have been (off) withheld. With that information, the layer
 * can take measures against hanging keys.
 * TODO: Should this be modeled as an event, or as an additional call on
 *       the UserInputEventHandler interface?
 */
class ConsoleEvent : public Event
{
public:
	explicit ConsoleEvent(EventType type);
};

} // namespace openmsx

#endif
