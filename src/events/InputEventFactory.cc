#include "InputEventFactory.hh"
#include "InputEvents.hh"
#include "CommandException.hh"
#include "StringOp.hh"
#include "TclObject.hh"
#include <stdexcept>
#include <SDL.h>

using std::make_shared;

namespace openmsx::InputEventFactory {

static EventPtr parseKeyEvent(std::string_view str, uint32_t unicode)
{
	auto keyCode = Keys::getCode(str);
	if (keyCode == Keys::K_NONE) {
		throw CommandException("Invalid keycode: ", str);
	}
	if (keyCode & Keys::KD_RELEASE) {
		return make_shared<KeyUpEvent>(keyCode);
	} else {
		return make_shared<KeyDownEvent>(keyCode, unicode);
	}
}

static EventPtr parseKeyEvent(const TclObject& str, Interpreter& interp)
{
	auto len = str.getListLength(interp);
	if (len == 1) {
		return make_shared<GroupEvent>(
			OPENMSX_KEY_GROUP_EVENT,
			std::vector<EventType>{OPENMSX_KEY_UP_EVENT, OPENMSX_KEY_DOWN_EVENT},
			makeTclList("keyb"));
	} else if (len == 2) {
		auto comp1 = str.getListIndex(interp, 1).getString();
		return parseKeyEvent(comp1, 0);
	} else if (len == 3) {
		auto comp1 = str.getListIndex(interp, 1).getString();
		auto comp2 = str.getListIndex(interp, 2).getString();
		if (StringOp::startsWith(comp2, "unicode")) {
			try {
				return parseKeyEvent(
					comp1, StringOp::fast_stou(comp2.substr(7)));
			} catch (std::invalid_argument&) {
				// parse error in fast_stou()
			}
		}
	}
	throw CommandException("Invalid keyboard event: ", str.getString());
}

static bool upDown(std::string_view str)
{
	if (str == "up") {
		return true;
	} else if (str == "down") {
		return false;
	}
	throw CommandException(
		"Invalid direction (expected 'up' or 'down'): ", str);
}

static EventPtr parseMouseEvent(const TclObject& str, Interpreter& interp)
{
	auto len = str.getListLength(interp);
	if (len >= 2) {
		auto comp1 = str.getListIndex(interp, 1).getString();
		if (comp1 == "motion") {
			if (len == 2) {
				return make_shared<GroupEvent>(
					OPENMSX_MOUSE_MOTION_GROUP_EVENT,
					std::vector<EventType>{OPENMSX_MOUSE_MOTION_EVENT},
					makeTclList("mouse", comp1));
			} else if ((len == 4) || (len == 6)) {
				int absX = 0, absY = 0;
				if (len == 6) {
					absX = str.getListIndex(interp, 4).getInt(interp);
					absY = str.getListIndex(interp, 5).getInt(interp);
				} else {
					// for bw-compat also allow events without absX,absY
				}
				return make_shared<MouseMotionEvent>(
					str.getListIndex(interp, 2).getInt(interp),
					str.getListIndex(interp, 3).getInt(interp),
					absX, absY);
			}
		} else if (StringOp::startsWith(comp1, "button")) {
			if (len == 2) {
				return make_shared<GroupEvent>(
					OPENMSX_MOUSE_BUTTON_GROUP_EVENT,
					std::vector<EventType>{OPENMSX_MOUSE_BUTTON_UP_EVENT, OPENMSX_MOUSE_BUTTON_DOWN_EVENT},
					makeTclList("mouse", "button"));
			} else if (len == 3) {
				try {
					unsigned button = StringOp::fast_stou(comp1.substr(6));
					if (upDown(str.getListIndex(interp, 2).getString())) {
						return make_shared<MouseButtonUpEvent>  (button);
					} else {
						return make_shared<MouseButtonDownEvent>(button);
					}
				} catch (std::invalid_argument&) {
					// parse error in fast_stou()
				}
			}
		} else if (comp1 == "wheel") {
			if (len == 2) {
				return make_shared<GroupEvent>(
					OPENMSX_MOUSE_WHEEL_GROUP_EVENT,
					std::vector<EventType>{OPENMSX_MOUSE_WHEEL_EVENT},
					makeTclList("mouse", comp1));
			} else if (len == 4) {
				return make_shared<MouseWheelEvent>(
					str.getListIndex(interp, 2).getInt(interp),
					str.getListIndex(interp, 3).getInt(interp));
			}
		}
	}
	throw CommandException("Invalid mouse event: ", str.getString());
}

static EventPtr parseOsdControlEvent(const TclObject& str, Interpreter& interp)
{
	if (str.getListLength(interp) == 3) {
		auto buttonName = str.getListIndex(interp, 1).getString();
		unsigned button;
		if (buttonName == "LEFT") {
			button = OsdControlEvent::LEFT_BUTTON;
		} else if (buttonName == "RIGHT") {
			button = OsdControlEvent::RIGHT_BUTTON;
		} else if (buttonName == "UP") {
			button = OsdControlEvent::UP_BUTTON;
		} else if (buttonName == "DOWN") {
			button = OsdControlEvent::DOWN_BUTTON;
		} else if (buttonName == "A") {
			button = OsdControlEvent::A_BUTTON;
		} else if (buttonName == "B") {
			button = OsdControlEvent::B_BUTTON;
		} else {
			goto error;
		}
		auto buttonAction = str.getListIndex(interp, 2).getString();
		if (buttonAction == "RELEASE") {
			return make_shared<OsdControlReleaseEvent>(button, nullptr);
		} else if (buttonAction == "PRESS") {
			return make_shared<OsdControlPressEvent>  (button, nullptr);
		}
	}
error:	throw CommandException("Invalid OSDcontrol event: ", str.getString());
}

static EventPtr parseJoystickEvent(const TclObject& str, Interpreter& interp)
{
	auto len = str.getListLength(interp);
	if (len >= 2) {
		auto comp0 = str.getListIndex(interp, 0).getString(); // joyN
		auto comp1 = str.getListIndex(interp, 1).getString();

		if (len == 2) {
			if (StringOp::startsWith(comp1, "button")) {
				return make_shared<GroupEvent>(
					OPENMSX_JOY_BUTTON_GROUP_EVENT,
					std::vector<EventType>{OPENMSX_JOY_BUTTON_UP_EVENT, OPENMSX_JOY_BUTTON_DOWN_EVENT},
					makeTclList("joy", "button"));
			} else if (StringOp::startsWith(comp1, "axis")) {
				return make_shared<GroupEvent>(
					OPENMSX_JOY_AXIS_MOTION_GROUP_EVENT,
					std::vector<EventType>{OPENMSX_JOY_AXIS_MOTION_EVENT},
					makeTclList("joy", "axis"));
			} else if (StringOp::startsWith(comp1, "hat")) {
				return make_shared<GroupEvent>(
					OPENMSX_JOY_HAT_GROUP_EVENT,
					std::vector<EventType>{OPENMSX_JOY_HAT_EVENT},
					makeTclList("joy", "hat"));
			}
		} else if (len == 3) {
			try {
				auto comp2 = str.getListIndex(interp, 2);
				unsigned joystick = StringOp::fast_stou(comp0.substr(3)) - 1;

				if (StringOp::startsWith(comp1, "button")) {
					unsigned button = StringOp::fast_stou(comp1.substr(6));
					if (upDown(comp2.getString())) {
						return make_shared<JoystickButtonUpEvent>  (joystick, button);
					} else {
						return make_shared<JoystickButtonDownEvent>(joystick, button);
					}
				} else if (StringOp::startsWith(comp1, "axis")) {
					unsigned axis = StringOp::fast_stou(comp1.substr(4));
					int value = str.getListIndex(interp, 2).getInt(interp);
					return make_shared<JoystickAxisMotionEvent>(joystick, axis, value);
				} else if (StringOp::startsWith(comp1, "hat")) {
					unsigned hat = StringOp::fast_stou(comp1.substr(3));
					auto valueStr = str.getListIndex(interp, 2).getString();
					int value;
					if      (valueStr == "up")        value = SDL_HAT_UP;
					else if (valueStr == "right")     value = SDL_HAT_RIGHT;
					else if (valueStr == "down")      value = SDL_HAT_DOWN;
					else if (valueStr == "left")      value = SDL_HAT_LEFT;
					else if (valueStr == "rightup")   value = SDL_HAT_RIGHTUP;
					else if (valueStr == "rightdown") value = SDL_HAT_RIGHTDOWN;
					else if (valueStr == "leftup")    value = SDL_HAT_LEFTUP;
					else if (valueStr == "leftdown")  value = SDL_HAT_LEFTDOWN;
					else if (valueStr == "center")    value = SDL_HAT_CENTERED;
					else {
						throw CommandException("Invalid hat value: ", valueStr);
					}
					return make_shared<JoystickHatEvent>(joystick, hat, value);
				}
			} catch (std::invalid_argument&) {
				// parse error in fast_stou()
			}
		}
	}
	throw CommandException("Invalid joystick event: ", str.getString());
}

static EventPtr parseFocusEvent(const TclObject& str, Interpreter& interp)
{
	if (str.getListLength(interp) != 2) {
		throw CommandException("Invalid focus event: ", str.getString());
	}
	return make_shared<FocusEvent>(str.getListIndex(interp, 1).getBoolean(interp));
}

static EventPtr parseResizeEvent(const TclObject& str, Interpreter& interp)
{
	if (str.getListLength(interp) != 3) {
		throw CommandException("Invalid resize event: ", str.getString());
	}
	return make_shared<ResizeEvent>(
		str.getListIndex(interp, 1).getInt(interp),
		str.getListIndex(interp, 2).getInt(interp));
}

static EventPtr parseQuitEvent(const TclObject& str, Interpreter& interp)
{
	if (str.getListLength(interp) != 1) {
		throw CommandException("Invalid quit event: ", str.getString());
	}
	return make_shared<QuitEvent>();
}

EventPtr createInputEvent(const TclObject& str, Interpreter& interp)
{
	if (str.getListLength(interp) == 0) {
		throw CommandException("Invalid event: ", str.getString());
	}
	auto type = str.getListIndex(interp, 0).getString();
	if (type == "keyb") {
		return parseKeyEvent(str, interp);
	} else if (type == "mouse") {
		return parseMouseEvent(str, interp);
	} else if (StringOp::startsWith(type, "joy")) {
		return parseJoystickEvent(str, interp);
	} else if (type == "focus") {
		return parseFocusEvent(str, interp);
	} else if (type == "resize") {
		return parseResizeEvent(str, interp);
	} else if (type == "quit") {
		return parseQuitEvent(str, interp);
	} else if (type == "command") {
		return EventPtr();
		//return parseCommandEvent(str);
	} else if (type == "OSDcontrol") {
		return parseOsdControlEvent(str, interp);
	} else {
		// fall back
		return parseKeyEvent(type, 0);
	}
}
EventPtr createInputEvent(std::string_view str, Interpreter& interp)
{
	return createInputEvent(TclObject(str), interp);
}

} // namespace openmsx::InputEventFactory
