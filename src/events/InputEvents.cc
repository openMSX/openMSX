// $Id$

#include "InputEvents.hh"
#include "Keys.hh"
#include "TclObject.hh"
#include "StringOp.hh"
#include "checked_cast.hh"
#include <string>
#include <cassert>

using std::string;
using std::vector;

namespace openmsx {

// class InputEvent

InputEvent::InputEvent(EventType type)
	: Event(type)
{
}

bool InputEvent::lessImpl(const Event& other) const
{
	const InputEvent* otherEvent = checked_cast<const InputEvent*>(&other);
	return (getType() != otherEvent->getType())
	     ? (getType() <  otherEvent->getType())
	     : lessImpl(*otherEvent);
}


// class KeyEvent

KeyEvent::KeyEvent(EventType type, Keys::KeyCode keyCode_, word unicode_)
	: InputEvent(type), keyCode(keyCode_), unicode(unicode_)
{
}

Keys::KeyCode KeyEvent::getKeyCode() const
{
	return keyCode;
}

word KeyEvent::getUnicode() const
{
	return unicode;
}

void KeyEvent::toStringImpl(TclObject& result) const
{
	result.addListElement("keyb");
	result.addListElement(Keys::getName(getKeyCode()));
}

bool KeyEvent::lessImpl(const InputEvent& other) const
{
	// note: don't compare unicode
	const KeyEvent* otherKeyEvent = checked_cast<const KeyEvent*>(&other);
	return getKeyCode() < otherKeyEvent->getKeyCode();
}


// class KeyUpEvent

KeyUpEvent::KeyUpEvent(Keys::KeyCode keyCode)
	: KeyEvent(OPENMSX_KEY_UP_EVENT, keyCode, (word)-1)
{
}

KeyUpEvent::KeyUpEvent(Keys::KeyCode keyCode, word unicode)
	: KeyEvent(OPENMSX_KEY_UP_EVENT, keyCode, unicode)
{
}


// class KeyDownEvent
 
KeyDownEvent::KeyDownEvent(Keys::KeyCode keyCode)
	: KeyEvent(OPENMSX_KEY_DOWN_EVENT, keyCode, (word)-1)
{
}

KeyDownEvent::KeyDownEvent(Keys::KeyCode keyCode, word unicode)
	: KeyEvent(OPENMSX_KEY_DOWN_EVENT, keyCode, unicode)
{
}


// class MouseButtonEvent

MouseButtonEvent::MouseButtonEvent(EventType type, unsigned button_)
	: InputEvent(type), button(button_)
{
}

unsigned MouseButtonEvent::getButton() const
{
	return button;
}

void MouseButtonEvent::toStringHelper(TclObject& result) const
{
	result.addListElement("mouse");
	result.addListElement("button" + StringOp::toString(getButton()));
}

bool MouseButtonEvent::lessImpl(const InputEvent& other) const
{
	const MouseButtonEvent* otherMouseEvent =
		checked_cast<const MouseButtonEvent*>(&other);
	return getButton() < otherMouseEvent->getButton();
}


// class MouseButtonUpEvent

MouseButtonUpEvent::MouseButtonUpEvent(unsigned button)
	: MouseButtonEvent(OPENMSX_MOUSE_BUTTON_UP_EVENT, button)
{
}

void MouseButtonUpEvent::toStringImpl(TclObject& result) const
{
	toStringHelper(result);
	result.addListElement("up");
}


// class MouseButtonDownEvent

MouseButtonDownEvent::MouseButtonDownEvent(unsigned button)
	: MouseButtonEvent(OPENMSX_MOUSE_BUTTON_DOWN_EVENT, button)
{
}

void MouseButtonDownEvent::toStringImpl(TclObject& result) const
{
	toStringHelper(result);
	result.addListElement("down");
}


// class MouseMotionEvent

MouseMotionEvent::MouseMotionEvent(int xrel_, int yrel_)
	: InputEvent(OPENMSX_MOUSE_MOTION_EVENT), xrel(xrel_), yrel(yrel_)
{
}

int MouseMotionEvent::getX() const
{
	return xrel;
}

int MouseMotionEvent::getY() const
{
	return yrel;
}

void MouseMotionEvent::toStringImpl(TclObject& result) const
{
	result.addListElement("mouse");
	result.addListElement("motion");
	result.addListElement(getX());
	result.addListElement(getY());
}

bool MouseMotionEvent::lessImpl(const InputEvent& other) const
{
	const MouseMotionEvent* otherMouseEvent =
		checked_cast<const MouseMotionEvent*>(&other);
	return (getX() != otherMouseEvent->getX())
	     ? (getX() <  otherMouseEvent->getX())
	     : (getY() <  otherMouseEvent->getY());
}


// class JoystickEvent

JoystickEvent::JoystickEvent(EventType type, unsigned joystick_)
	: InputEvent(type), joystick(joystick_)
{
}

unsigned JoystickEvent::getJoystick() const
{
	return joystick;
}

void JoystickEvent::toStringHelper(TclObject& result) const
{
	result.addListElement("joy" + StringOp::toString(getJoystick()));
}

bool JoystickEvent::lessImpl(const InputEvent& other) const
{
	const JoystickEvent* otherJoystickEvent =
		checked_cast<const JoystickEvent*>(&other);
	return (getJoystick() != otherJoystickEvent->getJoystick())
	     ? (getJoystick() <  otherJoystickEvent->getJoystick())
	     : lessImpl(*otherJoystickEvent);
}


// class JoystickButtonEvent

JoystickButtonEvent::JoystickButtonEvent(
		EventType type, unsigned joystick, unsigned button_)
	: JoystickEvent(type, joystick), button(button_)
{
}

unsigned JoystickButtonEvent::getButton() const
{
	return button;
}

void JoystickButtonEvent::toStringHelper(TclObject& result) const
{
	JoystickEvent::toStringHelper(result);
	result.addListElement("button" + StringOp::toString(getButton()));
}

bool JoystickButtonEvent::lessImpl(const JoystickEvent& other) const
{
	const JoystickButtonEvent* otherJoystickButtonEvent =
		checked_cast<const JoystickButtonEvent*>(&other);
	return getButton() < otherJoystickButtonEvent->getButton();
}


// class JoystickButtonUpEvent

JoystickButtonUpEvent::JoystickButtonUpEvent(unsigned joystick, unsigned button)
	: JoystickButtonEvent(OPENMSX_JOY_BUTTON_UP_EVENT, joystick, button)
{
}

void JoystickButtonUpEvent::toStringImpl(TclObject& result) const
{
	toStringHelper(result);
	result.addListElement("up");
}


// class JoystickButtonDownEvent

JoystickButtonDownEvent::JoystickButtonDownEvent(unsigned joystick, unsigned button)
	: JoystickButtonEvent(OPENMSX_JOY_BUTTON_DOWN_EVENT, joystick, button)
{
}

void JoystickButtonDownEvent::toStringImpl(TclObject& result) const
{
	toStringHelper(result);
	result.addListElement("down");
}


// class JoystickAxisMotionEvent

JoystickAxisMotionEvent::JoystickAxisMotionEvent(
		unsigned joystick, unsigned axis_, short value_)
	: JoystickEvent(OPENMSX_JOY_AXIS_MOTION_EVENT, joystick)
	, axis(axis_), value(value_)
{
}

unsigned JoystickAxisMotionEvent::getAxis() const
{
	return axis;
}

short JoystickAxisMotionEvent::getValue() const
{
	return value;
}

void JoystickAxisMotionEvent::toStringImpl(TclObject& result) const
{
	toStringHelper(result);
	result.addListElement("axis" + StringOp::toString(getAxis()));
	result.addListElement(getValue());
}

bool JoystickAxisMotionEvent::lessImpl(const JoystickEvent& other) const
{
	const JoystickAxisMotionEvent* otherJoystickAxisMotionEvent =
		checked_cast<const JoystickAxisMotionEvent*>(&other);
	return (getAxis() != otherJoystickAxisMotionEvent->getAxis())
	     ? (getAxis() <  otherJoystickAxisMotionEvent->getAxis())
	     : (getValue() < otherJoystickAxisMotionEvent->getValue());
}


// class FocusEvent

FocusEvent::FocusEvent(bool gain_)
	: InputEvent(OPENMSX_FOCUS_EVENT), gain(gain_)
{
}

bool FocusEvent::getGain() const
{
	return gain;
}

void FocusEvent::toStringImpl(TclObject& result) const
{
	result.addListElement("focus");
	result.addListElement(getGain());
}

bool FocusEvent::lessImpl(const InputEvent& other) const
{
	const FocusEvent* otherFocusEvent =
		checked_cast<const FocusEvent*>(&other);
	return getGain() < otherFocusEvent->getGain();
}


// class ResizeEvent

ResizeEvent::ResizeEvent(unsigned x_, unsigned y_)
	: InputEvent(OPENMSX_RESIZE_EVENT), x(x_), y(y_)
{
}

unsigned ResizeEvent::getX() const
{
	return x;
}

unsigned ResizeEvent::getY() const
{
	return y;
}

void ResizeEvent::toStringImpl(TclObject& result) const
{
	result.addListElement("resize");
	result.addListElement(static_cast<int>(getX()));
	result.addListElement(static_cast<int>(getY()));
}

bool ResizeEvent::lessImpl(const InputEvent& other) const
{
	const ResizeEvent* otherResizeEvent =
		checked_cast<const ResizeEvent*>(&other);
	return (getX() != otherResizeEvent->getX())
	     ? (getX() <  otherResizeEvent->getX())
	     : (getY() <  otherResizeEvent->getY());
}


// class QuitEvent

QuitEvent::QuitEvent() : InputEvent(OPENMSX_QUIT_EVENT)
{
}

void QuitEvent::toStringImpl(TclObject& result) const
{
	result.addListElement("quit");
}

bool QuitEvent::lessImpl(const InputEvent& /*other*/) const
{
	return false;
}


// class MSXCommandEvent

MSXCommandEvent::MSXCommandEvent(const vector<string>& tokens_)
	: InputEvent(OPENMSX_MSX_COMMAND_EVENT)
	, tokens(tokens_)
{
}

const vector<string>& MSXCommandEvent::getTokens() const
{
	return tokens;
}

void MSXCommandEvent::toStringImpl(TclObject& result) const
{
	result.addListElement("command");
	result.addListElements(tokens.begin(), tokens.end());
}

bool MSXCommandEvent::lessImpl(const InputEvent& other) const
{
	const MSXCommandEvent* otherCommandEvent =
		checked_cast<const MSXCommandEvent*>(&other);
	return getTokens() < otherCommandEvent->getTokens();
}


// class ConsoleEvent

ConsoleEvent::ConsoleEvent(EventType type)
	: Event(type)
{
}

} // namespace openmsx
