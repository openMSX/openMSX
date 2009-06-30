// $Id$

#ifndef INPUTEVENTS_HH
#define INPUTEVENTS_HH

#include "openmsx.hh"
#include "Event.hh"
#include "Keys.hh"
#include <string>
#include <vector>

namespace openmsx {

class InputEvent : public Event
{
protected:
	explicit InputEvent(EventType type);
};


class KeyEvent : public Event
{
public:
	Keys::KeyCode getKeyCode() const;
	word getUnicode() const;

protected:
	KeyEvent(EventType type, Keys::KeyCode keyCode, word unicode);

private:
	virtual void toStringImpl(TclObject& result) const;
	virtual bool lessImpl(const Event& other) const;
	const Keys::KeyCode keyCode;
	const word unicode;
};

class KeyUpEvent : public KeyEvent
{
public:
	explicit KeyUpEvent(Keys::KeyCode keyCode);
	KeyUpEvent(Keys::KeyCode keyCode, word unicode);
};

class KeyDownEvent : public KeyEvent
{
public:
	explicit KeyDownEvent(Keys::KeyCode keyCode);
	KeyDownEvent(Keys::KeyCode keyCode, word unicode);
};


class MouseButtonEvent : public Event
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
	void toStringHelper(TclObject& result) const;

private:
	virtual bool lessImpl(const Event& other) const;
	const unsigned button;
};

class MouseButtonUpEvent : public MouseButtonEvent
{
public:
	explicit MouseButtonUpEvent(unsigned button);
private:
	virtual void toStringImpl(TclObject& result) const;
};

class MouseButtonDownEvent : public MouseButtonEvent
{
public:
	explicit MouseButtonDownEvent(unsigned button);
private:
	virtual void toStringImpl(TclObject& result) const;
};

class MouseMotionEvent : public Event
{
public:
	MouseMotionEvent(int xrel, int yrel);
	int getX() const;
	int getY() const;

private:
	virtual void toStringImpl(TclObject& result) const;
	virtual bool lessImpl(const Event& other) const;
	const int xrel;
	const int yrel;
};


class JoystickEvent : public Event
{
public:
	unsigned getJoystick() const;

protected:
	JoystickEvent(EventType type, unsigned joystick);
	void toStringHelper(TclObject& result) const;

private:
	virtual bool lessImpl(const Event& other) const;
	virtual bool lessImpl(const JoystickEvent& other) const = 0;
	const unsigned joystick;
};

class JoystickButtonEvent : public JoystickEvent
{
public:
	unsigned getButton() const;

protected:
	JoystickButtonEvent(EventType type, unsigned joystick, unsigned button);
	void toStringHelper(TclObject& result) const;

private:
	virtual bool lessImpl(const JoystickEvent& other) const;
	const unsigned button;
};

class JoystickButtonUpEvent : public JoystickButtonEvent
{
public:
	JoystickButtonUpEvent(unsigned joystick, unsigned button);
private:
	virtual void toStringImpl(TclObject& result) const;
};

class JoystickButtonDownEvent : public JoystickButtonEvent
{
public:
	JoystickButtonDownEvent(unsigned joystick, unsigned button);
private:
	virtual void toStringImpl(TclObject& result) const;
};

class JoystickAxisMotionEvent : public JoystickEvent
{
public:
	static const unsigned X_AXIS = 0;
	static const unsigned Y_AXIS = 1;

	JoystickAxisMotionEvent(unsigned joystick, unsigned axis, short value);
	unsigned getAxis() const;
	short getValue() const;

private:
	virtual void toStringImpl(TclObject& result) const;
	virtual bool lessImpl(const JoystickEvent& other) const;
	const unsigned axis;
	const short value;
};


class FocusEvent : public Event
{
public:
	explicit FocusEvent(bool gain);

	bool getGain() const;

private:
	virtual void toStringImpl(TclObject& result) const;
	virtual bool lessImpl(const Event& other) const;
	const bool gain;
};


class ResizeEvent : public Event
{
public:
	ResizeEvent(unsigned x, unsigned y);

	unsigned getX() const;
	unsigned getY() const;

private:
	virtual void toStringImpl(TclObject& result) const;
	virtual bool lessImpl(const Event& other) const;
	const unsigned x;
	const unsigned y;
};


class QuitEvent : public Event
{
public:
	QuitEvent();
private:
	virtual void toStringImpl(TclObject& result) const;
	virtual bool lessImpl(const Event& other) const;
};


/** This class is used to for Tcl commands that directly influence the MSX
  * state (e.g. plug, disk<x>, cassetteplayer, reset). It's passed via an
  * event because the recording needs to see these.
  */
class MSXCommandEvent : public Event
{
public:
	explicit MSXCommandEvent(const std::vector<std::string>& tokens);
	explicit MSXCommandEvent(const std::vector<TclObject*>& tokens);
	virtual ~MSXCommandEvent();
	const std::vector<TclObject*>& getTokens() const;
private:
	virtual void toStringImpl(TclObject& result) const;
	virtual bool lessImpl(const Event& other) const;
	std::vector<TclObject*> tokens;
	const bool owned;
};

} // namespace openmsx

#endif
