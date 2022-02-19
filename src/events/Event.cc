#include "Event.hh"
#include "stl.hh"
#include "strCat.hh"
#include <tuple>
#include <SDL.h>

using namespace std::literals;

namespace openmsx {

bool operator==(const Event& x, const Event& y)
{
	return visit(overloaded{
		[](const KeyUpEvent& a, const KeyUpEvent& b) {
			// note: don't compare scancode, unicode
			return a.getKeyCode() == b.getKeyCode();
		},
		[](const KeyDownEvent& a, const KeyDownEvent& b) {
			// note: don't compare scancode, unicode
			return a.getKeyCode() == b.getKeyCode();
		},
		[](const MouseMotionEvent& a, const MouseMotionEvent& b) {
			return std::tuple(a.getX(), a.getY(), a.getAbsX(), a.getAbsY()) ==
			       std::tuple(b.getX(), b.getY(), b.getAbsX(), b.getAbsY());
		},
		[](const MouseButtonUpEvent& a, const MouseButtonUpEvent& b) {
			return a.getButton() == b.getButton();
		},
		[](const MouseButtonDownEvent& a, const MouseButtonDownEvent& b) {
			return a.getButton() == b.getButton();
		},
		[](const MouseWheelEvent& a, const MouseWheelEvent& b) {
			return std::tuple(a.getX(), a.getY()) ==
			       std::tuple(b.getX(), b.getY());
		},
		[](const JoystickAxisMotionEvent& a, const JoystickAxisMotionEvent& b) {
			return std::tuple(a.getJoystick(), a.getAxis(), a.getValue()) ==
			       std::tuple(b.getJoystick(), b.getAxis(), b.getValue());
		},
		[](const JoystickHatEvent& a, const JoystickHatEvent& b) {
			return std::tuple(a.getJoystick(), a.getHat(), a.getValue()) ==
			       std::tuple(b.getJoystick(), b.getHat(), b.getValue());
		},
		[](const JoystickButtonUpEvent& a, const JoystickButtonUpEvent& b) {
			return std::tuple(a.getJoystick(), a.getButton()) ==
			       std::tuple(b.getJoystick(), b.getButton());
		},
		[](const JoystickButtonDownEvent& a, const JoystickButtonDownEvent& b) {
			return std::tuple(a.getJoystick(), a.getButton()) ==
			       std::tuple(b.getJoystick(), b.getButton());
		},
		[](const OsdControlReleaseEvent& a, const OsdControlReleaseEvent& b) {
			return a.getButton() == b.getButton();
		},
		[](const OsdControlPressEvent& a, const OsdControlPressEvent& b) {
			return a.getButton() == b.getButton();
		},
		[](const FocusEvent& a, const FocusEvent& b) {
			return a.getGain() == b.getGain();
		},
		[](const ResizeEvent& a, const ResizeEvent& b) {
			return std::tuple(a.getX(), a.getY()) ==
			       std::tuple(b.getX(), b.getY());
		},
		[](const FileDropEvent& a, const FileDropEvent& b) {
			return a.getFileName() == b.getFileName();
		},
		[](const FinishFrameEvent& a, const FinishFrameEvent& b) {
			return std::tuple(a.getSource(), a.getSelectedSource(), a.isSkipped()) ==
			       std::tuple(b.getSource(), b.getSelectedSource(), b.isSkipped());
		},
		[](const CliCommandEvent& a, const CliCommandEvent& b) {
			return a.getCommand() == b.getCommand();
		},
		[](const GroupEvent& a, const GroupEvent& b) {
			return a.getTclListComponents() ==
			       b.getTclListComponents();
		},
		[&](const EventBase& /*a*/, const EventBase& /*b*/) {
			return getType(x) == getType(y);
		}
	}, x, y);
}

TclObject toTclList(const Event& event)
{
	static constexpr std::array osdControlNames = {
		"LEFT"sv, "RIGHT"sv, "UP"sv, "DOWN"sv, "A"sv, "B"sv
	};

	return visit(overloaded{
		[](const KeyEvent& e) {
			// Note: 'scanCode' is not included (also not in operator==()).
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
			auto result = makeTclList("keyb", Keys::getName(e.getKeyCode()));
			if (e.getUnicode() != 0) {
				result.addListElement(tmpStrCat("unicode", e.getUnicode()));
			}
			return result;
		},
		[](const MouseMotionEvent& e) {
			return makeTclList("mouse", "motion", e.getX(), e.getY(), e.getAbsX(), e.getAbsY());
		},
		[](const MouseButtonUpEvent& e) {
			return makeTclList("mouse", tmpStrCat("button", e.getButton()), "up");
		},
		[](const MouseButtonDownEvent& e) {
			return makeTclList("mouse", tmpStrCat("button", e.getButton()), "down");
		},
		[](const MouseWheelEvent& e) {
			return makeTclList("mouse", "wheel", e.getX(), e.getY());
		},
		[](const JoystickAxisMotionEvent& e) {
			return makeTclList(tmpStrCat("joy", e.getJoystick() + 1), tmpStrCat("axis", e.getAxis()), e.getValue());
		},
		[](const JoystickHatEvent& e) {
			const char* str = [&] {
				switch (e.getValue()) {
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
			return makeTclList(tmpStrCat("joy", e.getJoystick() + 1), tmpStrCat("hat", e.getHat()), str);
		},
		[](const JoystickButtonUpEvent& e) {
			return makeTclList(tmpStrCat("joy", e.getJoystick() + 1), tmpStrCat("button", e.getButton()), "up");
		},
		[](const JoystickButtonDownEvent& e) {
			return makeTclList(tmpStrCat("joy", e.getJoystick() + 1), tmpStrCat("button", e.getButton()), "down");
		},
		[](const OsdControlReleaseEvent& e) {
			return makeTclList("OSDcontrol", osdControlNames[e.getButton()], "RELEASE");
		},
		[](const OsdControlPressEvent& e) {
			return makeTclList("OSDcontrol", osdControlNames[e.getButton()], "PRESS");
		},
		[](const FocusEvent& e) {
			return makeTclList("focus", e.getGain());
		},
		[](const ResizeEvent& e) {
			return makeTclList("resize", int(e.getX()), int(e.getY()));
		},
		[](const FileDropEvent& e) {
			return makeTclList("filedrop", e.getFileName());
		},
		[](const QuitEvent& /*e*/) {
			return makeTclList("quit");
		},
		[](const FinishFrameEvent& e) {
			return makeTclList("finishframe", int(e.getSource()), int(e.getSelectedSource()), e.isSkipped());
		},
		[](const CliCommandEvent& e) {
			return makeTclList("CliCmd", e.getCommand());
		},
		[](const GroupEvent& e) {
			return e.getTclListComponents();
		},
		[&](const SimpleEvent& /*e*/) {
			return makeTclList("simple", int(getType(event)));
		}
	}, event);
}

std::string toString(const Event& event)
{
	return std::string(toTclList(event).getString());
}

bool isRepeatStopper(const Event& self, const Event& other)
{
	assert(self && other);
	return visit(overloaded{
		// Normally all events should stop the repeat process in 'bind -repeat',
		// but in case of OsdControlEvent there are two exceptions:
		//  - we should not stop because of the original host event that
		//    actually generated this 'artificial' OsdControlEvent.
		//  - if the original host event is a joystick motion event, we
		//    should not stop repeat for 'small' relative new joystick events.
		[&](const OsdControlEvent& e) {
			// If this OsdControlEvent was generated by the other event, then
			// repeat should not be stopped.
			if (!e.getOrigEvent()) return true;
			if (e.getOrigEvent() == other) return false;

			// If this OsdControlEvent event was generated by a joystick motion
			// event and the new event is also a joystick motion event then don't
			// stop repeat. We don't need to check the actual values of the events
			// (it also isn't trivial), because when the values differ by enough,
			// a new OsdControlEvent will be generated and that one will stop
			// repeat.
			return (getType(e.getOrigEvent()) != EventType::JOY_AXIS_MOTION) ||
			       (getType(other)            != EventType::JOY_AXIS_MOTION);
		},
		[](const EventBase& /*e*/) {
			return true;
		}
	}, self);
}

bool matches(const Event& self, const Event& other)
{
	assert(self && other);
	return visit(overloaded{
		[&](const GroupEvent& e) {
			return contains(e.getTypesToMatch(), getType(other));
		},
		[&](const EventBase&) {
			return self == other;
		}
	}, self);
}

} // namespace openmsx
