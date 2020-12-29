#include "InputEvents.hh"
#include "Keys.hh"
#include "TclObject.hh"
#include "Timer.hh"
#include "checked_cast.hh"
#include "strCat.hh"
#include "stl.hh"
#include <string>
#include <tuple>
#include <SDL.h>

using std::string;

namespace openmsx {

// class TimedEvent

TimedEvent::TimedEvent(EventType type_)
	: Event(type_)
	, realtime(Timer::getTime()) // TODO use SDL2 event timestamp
{
}


// class KeyEvent

KeyEvent::KeyEvent(EventType type_, Keys::KeyCode keyCode_, Keys::KeyCode scanCode_, uint32_t unicode_)
	: TimedEvent(type_), keyCode(keyCode_), scanCode(scanCode_), unicode(unicode_)
{
}

TclObject KeyEvent::toTclList() const
{
	// Note: 'scanCode' is not included (also not in lessImpl()).
	//
	// At the moment 'scanCode' is only used in the MSX Keyboard code when
	// the POSITIONAL mapping mode is active. It is not used for key
	// bindings (the 'bind' or the 'after' commands) or for the openMSX
	// console. KeyEvents also don't end up in 'reverse replay' files
	// (instead those files contain more low level MSX keyboard matrix
	// changes).
	//
	// Within these constraints it's fine to ignore 'scanCode' in this
	// method.
	auto result = makeTclList("keyb", Keys::getName(getKeyCode()));
	if (getUnicode() != 0) {
		result.addListElement(tmpStrCat("unicode", getUnicode()));
	}
	return result;
}

bool KeyEvent::lessImpl(const Event& other) const
{
	// note: don't compare scancode, unicode
	const auto& o = checked_cast<const KeyEvent&>(other);
	return getKeyCode() < o.getKeyCode();
}


// class MouseButtonEvent

MouseButtonEvent::MouseButtonEvent(EventType type_, unsigned button_)
	: TimedEvent(type_), button(button_)
{
}

TclObject MouseButtonEvent::toTclHelper(std::string_view direction) const
{
	return makeTclList("mouse", tmpStrCat("button", getButton()), direction);
}

bool MouseButtonEvent::lessImpl(const Event& other) const
{
	const auto& o = checked_cast<const MouseButtonEvent&>(other);
	return getButton() < o.getButton();
}


// class MouseButtonUpEvent

MouseButtonUpEvent::MouseButtonUpEvent(unsigned button_)
	: MouseButtonEvent(OPENMSX_MOUSE_BUTTON_UP_EVENT, button_)
{
}

TclObject MouseButtonUpEvent::toTclList() const
{
	return toTclHelper("up");
}


// class MouseButtonDownEvent

MouseButtonDownEvent::MouseButtonDownEvent(unsigned button_)
	: MouseButtonEvent(OPENMSX_MOUSE_BUTTON_DOWN_EVENT, button_)
{
}

TclObject MouseButtonDownEvent::toTclList() const
{
	return toTclHelper("down");
}


// class MouseWheelEvent

MouseWheelEvent::MouseWheelEvent(int x_, int y_)
	: TimedEvent(OPENMSX_MOUSE_WHEEL_EVENT)
	, x(x_), y(y_)
{
}

TclObject MouseWheelEvent::toTclList() const
{
	return makeTclList("mouse", "wheel", getX(), getY());
}

bool MouseWheelEvent::lessImpl(const Event& other) const
{
	const auto& o = checked_cast<const MouseWheelEvent&>(other);
	return std::tuple(  getX(),   getY()) <
	       std::tuple(o.getX(), o.getY());
}


// class MouseMotionEvent

MouseMotionEvent::MouseMotionEvent(int xrel_, int yrel_, int xabs_, int yabs_)
	: TimedEvent(OPENMSX_MOUSE_MOTION_EVENT)
	, xrel(xrel_), yrel(yrel_)
	, xabs(xabs_), yabs(yabs_)
{
}

TclObject MouseMotionEvent::toTclList() const
{
	return makeTclList("mouse", "motion", getX(), getY(), getAbsX(), getAbsY());
}

bool MouseMotionEvent::lessImpl(const Event& other) const
{
	const auto& o = checked_cast<const MouseMotionEvent&>(other);
	return std::tuple(  getX(),   getY(),   getAbsX(),   getAbsY()) <
	       std::tuple(o.getX(), o.getY(), o.getAbsX(), o.getAbsY());
}


// class JoystickEvent

JoystickEvent::JoystickEvent(EventType type_, unsigned joystick_)
	: TimedEvent(type_), joystick(joystick_)
{
}

TclObject JoystickEvent::toTclHelper() const
{
	return makeTclList(tmpStrCat("joy", getJoystick() + 1));
}

bool JoystickEvent::lessImpl(const Event& other) const
{
	const auto& o = checked_cast<const JoystickEvent&>(other);
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

TclObject JoystickButtonEvent::toTclHelper(std::string_view direction) const
{
	auto result = JoystickEvent::toTclHelper();
	result.addListElement(tmpStrCat("button", getButton()), direction);
	return result;
}

bool JoystickButtonEvent::lessImpl(const JoystickEvent& other) const
{
	const auto& o = checked_cast<const JoystickButtonEvent&>(other);
	return getButton() < o.getButton();
}


// class JoystickButtonUpEvent

JoystickButtonUpEvent::JoystickButtonUpEvent(unsigned joystick_, unsigned button_)
	: JoystickButtonEvent(OPENMSX_JOY_BUTTON_UP_EVENT, joystick_, button_)
{
}

TclObject JoystickButtonUpEvent::toTclList() const
{
	return toTclHelper("up");
}


// class JoystickButtonDownEvent

JoystickButtonDownEvent::JoystickButtonDownEvent(unsigned joystick_, unsigned button_)
	: JoystickButtonEvent(OPENMSX_JOY_BUTTON_DOWN_EVENT, joystick_, button_)
{
}

TclObject JoystickButtonDownEvent::toTclList() const
{
	return toTclHelper("down");
}


// class JoystickAxisMotionEvent

JoystickAxisMotionEvent::JoystickAxisMotionEvent(
		unsigned joystick_, unsigned axis_, int value_)
	: JoystickEvent(OPENMSX_JOY_AXIS_MOTION_EVENT, joystick_)
	, axis(axis_), value(value_)
{
}

TclObject JoystickAxisMotionEvent::toTclList() const
{
	auto result = JoystickEvent::toTclHelper();
	result.addListElement(tmpStrCat("axis", getAxis()), getValue());
	return result;
}

bool JoystickAxisMotionEvent::lessImpl(const JoystickEvent& other) const
{
	const auto& o = checked_cast<const JoystickAxisMotionEvent&>(other);
	return std::tuple(  getAxis(),   getValue()) <
	       std::tuple(o.getAxis(), o.getValue());
}


// class JoystickHatEvent

JoystickHatEvent::JoystickHatEvent(unsigned joystick_, unsigned hat_, unsigned value_)
	: JoystickEvent(OPENMSX_JOY_HAT_EVENT, joystick_)
	, hat(hat_), value(value_)
{
}

TclObject JoystickHatEvent::toTclList() const
{
	auto result = JoystickEvent::toTclHelper();
	const char* str = [&] {
		switch (getValue()) {
			case SDL_HAT_UP:        return "up";
			case SDL_HAT_RIGHT:     return "right";
			case SDL_HAT_DOWN:      return "down";
			case SDL_HAT_LEFT:      return "left";
			case SDL_HAT_RIGHTUP:   return "rightup";
			case SDL_HAT_RIGHTDOWN: return "rightdown";
			case SDL_HAT_LEFTUP:    return "leftup";
			case SDL_HAT_LEFTDOWN:  return "leftdown";
			default:                return "center";
		}
	}();
	result.addListElement(tmpStrCat("hat", getHat()), str);
	return result;
}

bool JoystickHatEvent::lessImpl(const JoystickEvent& other) const
{
	const auto& o = checked_cast<const JoystickHatEvent&>(other);
	return std::tuple(  getHat(),   getValue()) <
	       std::tuple(o.getHat(), o.getValue());
}


// class FocusEvent

FocusEvent::FocusEvent(bool gain_)
	: Event(OPENMSX_FOCUS_EVENT), gain(gain_)
{
}

TclObject FocusEvent::toTclList() const
{
	return makeTclList("focus", getGain());
}

bool FocusEvent::lessImpl(const Event& other) const
{
	const auto& o = checked_cast<const FocusEvent&>(other);
	return getGain() < o.getGain();
}


// class ResizeEvent

ResizeEvent::ResizeEvent(unsigned x_, unsigned y_)
	: Event(OPENMSX_RESIZE_EVENT), x(x_), y(y_)
{
}

TclObject ResizeEvent::toTclList() const
{
	return makeTclList("resize", int(getX()), int(getY()));
}

bool ResizeEvent::lessImpl(const Event& other) const
{
	const auto& o = checked_cast<const ResizeEvent&>(other);
	return std::tuple(  getX(),   getY()) <
	       std::tuple(o.getX(), o.getY());
}


// class FileDropEvent

FileDropEvent::FileDropEvent(std::string fileName_)
	: Event(OPENMSX_FILEDROP_EVENT), fileName(std::move(fileName_))
{
}

TclObject FileDropEvent::toTclList() const
{
	return makeTclList("filedrop", fileName);
}

bool FileDropEvent::lessImpl(const Event& other) const
{
	const auto& o = checked_cast<const FileDropEvent&>(other);
	return getFileName() < o.getFileName();
}


// class QuitEvent

QuitEvent::QuitEvent() : Event(OPENMSX_QUIT_EVENT)
{
}

TclObject QuitEvent::toTclList() const
{
	return makeTclList("quit");
}

bool QuitEvent::lessImpl(const Event& /*other*/) const
{
	return false;
}

// class OsdControlEvent

OsdControlEvent::OsdControlEvent(
		EventType type_, unsigned button_,
		std::shared_ptr<const Event> origEvent_)
	: TimedEvent(type_), origEvent(std::move(origEvent_)), button(button_)
{
}

bool OsdControlEvent::isRepeatStopper(const Event& other) const
{
	// If this OsdControlEvent was generated by the other event, then
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

TclObject OsdControlEvent::toTclHelper(std::string_view direction) const
{
	static constexpr const char* const names[] = {
		"LEFT", "RIGHT", "UP", "DOWN", "A", "B"
	};
	return makeTclList("OSDcontrol", names[getButton()], direction);
}

bool OsdControlEvent::lessImpl(const Event& other) const
{
	const auto& o = checked_cast<const OsdControlEvent&>(other);
	return getButton() < o.getButton();
}


// class OsdControlReleaseEvent

OsdControlReleaseEvent::OsdControlReleaseEvent(
		unsigned button_, const std::shared_ptr<const Event>& origEvent_)
	: OsdControlEvent(OPENMSX_OSD_CONTROL_RELEASE_EVENT, button_, origEvent_)
{
}

TclObject OsdControlReleaseEvent::toTclList() const
{
	return toTclHelper("RELEASE");
}


// class OsdControlPressEvent

OsdControlPressEvent::OsdControlPressEvent(
		unsigned button_, const std::shared_ptr<const Event>& origEvent_)
	: OsdControlEvent(OPENMSX_OSD_CONTROL_PRESS_EVENT, button_, origEvent_)
{
}

TclObject OsdControlPressEvent::toTclList() const
{
	return toTclHelper("PRESS");
}


// class GroupEvent

GroupEvent::GroupEvent(EventType type_, std::vector<EventType> typesToMatch_, TclObject tclListComponents_)
	: Event(type_)
	, typesToMatch(std::move(typesToMatch_))
	, tclListComponents(std::move(tclListComponents_))
{
}

TclObject GroupEvent::toTclList() const
{
	return tclListComponents;
}

bool GroupEvent::lessImpl(const Event& /*other*/) const
{
	// All Group events are equivalent
	return false;
}

bool GroupEvent::matches(const Event& other) const
{
	return contains(typesToMatch, other.getType());
}


} // namespace openmsx
