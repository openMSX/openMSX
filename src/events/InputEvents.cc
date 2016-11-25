#include "InputEvents.hh"
#include "Keys.hh"
#include "TclObject.hh"
#include "StringOp.hh"
#include "Timer.hh"
#include "checked_cast.hh"
#include <string>
#include <tuple>
#include <SDL.h>

using std::make_tuple;
using std::string;
using std::vector;

namespace openmsx {

// class TimedEvent

TimedEvent::TimedEvent(EventType type_)
	: Event(type_)
	, realtime(Timer::getTime())
{
}


// class KeyEvent

KeyEvent::KeyEvent(EventType type_, Keys::KeyCode keyCode_)
	: TimedEvent(type_), keyCode(keyCode_)
{
}

void KeyEvent::toStringImpl(TclObject& result) const
{
	result.addListElement("keyb");
	result.addListElement(Keys::getName(getKeyCode()));
}

bool KeyEvent::lessImpl(const Event& other) const
{
	// note: don't compare unicode
	auto& o = checked_cast<const KeyEvent&>(other);
	return getKeyCode() < o.getKeyCode();
}


// class KeyUpEvent

KeyUpEvent::KeyUpEvent(Keys::KeyCode keyCode_)
	: KeyEvent(OPENMSX_KEY_UP_EVENT, keyCode_)
{
}


// class KeyDownEvent

KeyDownEvent::KeyDownEvent(Keys::KeyCode keyCode_)
	: KeyEvent(OPENMSX_KEY_DOWN_EVENT, keyCode_)
{
}


// class MouseButtonEvent

MouseButtonEvent::MouseButtonEvent(EventType type_, unsigned button_)
	: TimedEvent(type_), button(button_)
{
}

void MouseButtonEvent::toStringHelper(TclObject& result) const
{
	result.addListElement("mouse");
	result.addListElement(StringOp::Builder() << "button" << getButton());
}

bool MouseButtonEvent::lessImpl(const Event& other) const
{
	auto& o = checked_cast<const MouseButtonEvent&>(other);
	return getButton() < o.getButton();
}


// class MouseButtonUpEvent

MouseButtonUpEvent::MouseButtonUpEvent(unsigned button_)
	: MouseButtonEvent(OPENMSX_MOUSE_BUTTON_UP_EVENT, button_)
{
}

void MouseButtonUpEvent::toStringImpl(TclObject& result) const
{
	toStringHelper(result);
	result.addListElement("up");
}


// class MouseButtonDownEvent

MouseButtonDownEvent::MouseButtonDownEvent(unsigned button_)
	: MouseButtonEvent(OPENMSX_MOUSE_BUTTON_DOWN_EVENT, button_)
{
}

void MouseButtonDownEvent::toStringImpl(TclObject& result) const
{
	toStringHelper(result);
	result.addListElement("down");
}


// class MouseMotionEvent

MouseMotionEvent::MouseMotionEvent(int xrel_, int yrel_, int xabs_, int yabs_)
	: TimedEvent(OPENMSX_MOUSE_MOTION_EVENT)
	, xrel(xrel_), yrel(yrel_)
	, xabs(xabs_), yabs(yabs_)
{
}

void MouseMotionEvent::toStringImpl(TclObject& result) const
{
	result.addListElement("mouse");
	result.addListElement("motion");
	result.addListElement(getX());
	result.addListElement(getY());
	result.addListElement(getAbsX());
	result.addListElement(getAbsY());
}

bool MouseMotionEvent::lessImpl(const Event& other) const
{
	auto& o = checked_cast<const MouseMotionEvent&>(other);
	return make_tuple(  getX(),   getY(),   getAbsX(),   getAbsY()) <
	       make_tuple(o.getX(), o.getY(), o.getAbsX(), o.getAbsY());
}


// class MouseMotionGroupEvent

MouseMotionGroupEvent::MouseMotionGroupEvent()
	: Event(OPENMSX_MOUSE_MOTION_GROUP_EVENT)
{
}

void MouseMotionGroupEvent::toStringImpl(TclObject& result) const
{
	result.addListElement("mouse");
	result.addListElement("motion");
}

bool MouseMotionGroupEvent::lessImpl(const Event& /*other*/) const
{
	// All MouseMotionGroup events are equivalent
	return false;
}

bool MouseMotionGroupEvent::matches(const Event& other) const
{
	return other.getType() == OPENMSX_MOUSE_MOTION_EVENT;
}


// class JoystickEvent

JoystickEvent::JoystickEvent(EventType type_, unsigned joystick_)
	: TimedEvent(type_), joystick(joystick_)
{
}

void JoystickEvent::toStringHelper(TclObject& result) const
{
	result.addListElement(StringOp::Builder() << "joy" << getJoystick() + 1);
}

bool JoystickEvent::lessImpl(const Event& other) const
{
	auto& o = checked_cast<const JoystickEvent&>(other);
	return (getJoystick() != o.getJoystick())
	     ? (getJoystick() <  o.getJoystick())
	     : lessImpl(o);
}


// class JoystickButtonEvent

JoystickButtonEvent::JoystickButtonEvent(
		EventType type_, unsigned joystick_, unsigned button_)
	: JoystickEvent(type_, joystick_), button(button_)
{
}

void JoystickButtonEvent::toStringHelper(TclObject& result) const
{
	JoystickEvent::toStringHelper(result);
	result.addListElement(StringOp::Builder() << "button" << getButton());
}

bool JoystickButtonEvent::lessImpl(const JoystickEvent& other) const
{
	auto& o = checked_cast<const JoystickButtonEvent&>(other);
	return getButton() < o.getButton();
}


// class JoystickButtonUpEvent

JoystickButtonUpEvent::JoystickButtonUpEvent(unsigned joystick_, unsigned button_)
	: JoystickButtonEvent(OPENMSX_JOY_BUTTON_UP_EVENT, joystick_, button_)
{
}

void JoystickButtonUpEvent::toStringImpl(TclObject& result) const
{
	toStringHelper(result);
	result.addListElement("up");
}


// class JoystickButtonDownEvent

JoystickButtonDownEvent::JoystickButtonDownEvent(unsigned joystick_, unsigned button_)
	: JoystickButtonEvent(OPENMSX_JOY_BUTTON_DOWN_EVENT, joystick_, button_)
{
}

void JoystickButtonDownEvent::toStringImpl(TclObject& result) const
{
	toStringHelper(result);
	result.addListElement("down");
}


// class JoystickAxisMotionEvent

JoystickAxisMotionEvent::JoystickAxisMotionEvent(
		unsigned joystick_, unsigned axis_, int value_)
	: JoystickEvent(OPENMSX_JOY_AXIS_MOTION_EVENT, joystick_)
	, axis(axis_), value(value_)
{
}

void JoystickAxisMotionEvent::toStringImpl(TclObject& result) const
{
	toStringHelper(result);
	result.addListElement(StringOp::Builder() << "axis" << getAxis());
	result.addListElement(getValue());
}

bool JoystickAxisMotionEvent::lessImpl(const JoystickEvent& other) const
{
	auto& o = checked_cast<const JoystickAxisMotionEvent&>(other);
	return make_tuple(  getAxis(),   getValue()) <
	       make_tuple(o.getAxis(), o.getValue());
}


// class JoystickHatEvent

JoystickHatEvent::JoystickHatEvent(unsigned joystick_, unsigned hat_, unsigned value_)
	: JoystickEvent(OPENMSX_JOY_HAT_EVENT, joystick_)
	, hat(hat_), value(value_)
{
}

void JoystickHatEvent::toStringImpl(TclObject& result) const
{
	toStringHelper(result);
	result.addListElement(StringOp::Builder() << "hat" << getHat());
	const char* str;
	switch (getValue()) {
		case SDL_HAT_UP:        str = "up";        break;
		case SDL_HAT_RIGHT:     str = "right";     break;
		case SDL_HAT_DOWN:      str = "down";      break;
		case SDL_HAT_LEFT:      str = "left";      break;
		case SDL_HAT_RIGHTUP:   str = "rightup";   break;
		case SDL_HAT_RIGHTDOWN: str = "rightdown"; break;
		case SDL_HAT_LEFTUP:    str = "leftup";    break;
		case SDL_HAT_LEFTDOWN:  str = "leftdown";  break;
		default:                str = "center";    break;
	}
	result.addListElement(str);
}

bool JoystickHatEvent::lessImpl(const JoystickEvent& other) const
{
	auto& o = checked_cast<const JoystickHatEvent&>(other);
	return make_tuple(  getHat(),   getValue()) <
	       make_tuple(o.getHat(), o.getValue());
}


// class TextEvent

TextEvent::TextEvent(const string& text_)
	: Event(OPENMSX_TEXT_EVENT), text(text_)
{
}

void TextEvent::toStringImpl(TclObject& result) const
{
	result.addListElement("text");
	result.addListElement(getText());
}

bool TextEvent::lessImpl(const Event& other) const
{
	auto& o = checked_cast<const TextEvent&>(other);
	return getText() < o.getText();
}


// class FocusEvent

FocusEvent::FocusEvent(bool gain_)
	: Event(OPENMSX_FOCUS_EVENT), gain(gain_)
{
}

void FocusEvent::toStringImpl(TclObject& result) const
{
	result.addListElement("focus");
	result.addListElement(getGain());
}

bool FocusEvent::lessImpl(const Event& other) const
{
	auto& o = checked_cast<const FocusEvent&>(other);
	return getGain() < o.getGain();
}


// class ResizeEvent

ResizeEvent::ResizeEvent(unsigned x_, unsigned y_)
	: Event(OPENMSX_RESIZE_EVENT), x(x_), y(y_)
{
}

void ResizeEvent::toStringImpl(TclObject& result) const
{
	result.addListElement("resize");
	result.addListElement(int(getX()));
	result.addListElement(int(getY()));
}

bool ResizeEvent::lessImpl(const Event& other) const
{
	auto& o = checked_cast<const ResizeEvent&>(other);
	return make_tuple(  getX(),   getY()) <
	       make_tuple(o.getX(), o.getY());
}


// class QuitEvent

QuitEvent::QuitEvent() : Event(OPENMSX_QUIT_EVENT)
{
}

void QuitEvent::toStringImpl(TclObject& result) const
{
	result.addListElement("quit");
}

bool QuitEvent::lessImpl(const Event& /*other*/) const
{
	return false;
}

// class OsdControlEvent

OsdControlEvent::OsdControlEvent(
		EventType type_, unsigned button_,
		const std::shared_ptr<const Event>& origEvent_)
	: TimedEvent(type_), origEvent(origEvent_), button(button_)
{
}

bool OsdControlEvent::isRepeatStopper(const Event& other) const
{
	// If this OsdControlEvent was geneated by the other event, then
	// repeat should not be stopped.
	if (origEvent.get() == &other) return false;

	// If this OsdControlEvent event was generated by a joystick motion
	// event and the new event is also a joystick motion event then don't
	// stop repeat. We don't need to check the actual values of the events
	// (it also isn't trivial), because when the values differ by enough,
	// a new OsdControlEvent will be generated and that one will stop
	// repeat.
	return !dynamic_cast<const JoystickAxisMotionEvent*>(origEvent.get()) ||
	       !dynamic_cast<const JoystickAxisMotionEvent*>(&other);
}

void OsdControlEvent::toStringHelper(TclObject& result) const
{
	result.addListElement("OSDcontrol");
	static const char* const names[] = {
		"LEFT", "RIGHT", "UP", "DOWN", "A", "B"
	};
	result.addListElement(names[getButton()]);
}

bool OsdControlEvent::lessImpl(const Event& other) const
{
	auto& o = checked_cast<const OsdControlEvent&>(other);
	return getButton() < o.getButton();
}


// class OsdControlReleaseEvent

OsdControlReleaseEvent::OsdControlReleaseEvent(
		unsigned button_, const std::shared_ptr<const Event>& origEvent_)
	: OsdControlEvent(OPENMSX_OSD_CONTROL_RELEASE_EVENT, button_, origEvent_)
{
}

void OsdControlReleaseEvent::toStringImpl(TclObject& result) const
{
	toStringHelper(result);
	result.addListElement("RELEASE");
}


// class OsdControlPressEvent

OsdControlPressEvent::OsdControlPressEvent(
		unsigned button_, const std::shared_ptr<const Event>& origEvent_)
	: OsdControlEvent(OPENMSX_OSD_CONTROL_PRESS_EVENT, button_, origEvent_)
{
}

void OsdControlPressEvent::toStringImpl(TclObject& result) const
{
	toStringHelper(result);
	result.addListElement("PRESS");
}


} // namespace openmsx
