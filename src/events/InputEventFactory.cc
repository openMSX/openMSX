#include "InputEventFactory.hh"
#include "Event.hh"
#include "CommandException.hh"
#include "Keys.hh"
#include "StringOp.hh"
#include "TclObject.hh"
#include "one_of.hh"
#include <stdexcept>
#include <SDL.h>

namespace openmsx::InputEventFactory {

[[nodiscard]] static Event parseKeyEvent(std::string_view str, uint32_t unicode)
{
	auto keyCode = Keys::getCode(str);
	if (keyCode == Keys::K_NONE) {
		throw CommandException("Invalid keycode: ", str);
	}
	if (keyCode & Keys::KD_RELEASE) {
		return Event::create<KeyUpEvent>(keyCode);
	} else {
		return Event::create<KeyDownEvent>(keyCode, unicode);
	}
}

[[nodiscard]] static Event parseKeyEvent(const TclObject& str, Interpreter& interp)
{
	auto len = str.getListLength(interp);
	if (len == 1) {
		return Event::create<GroupEvent>(
			std::vector{EventType::KEY_UP, EventType::KEY_DOWN},
			makeTclList("keyb"));
	} else if (len == 2) {
		auto comp1 = str.getListIndex(interp, 1).getString();
		return parseKeyEvent(comp1, 0);
	} else if (len == 3) {
		auto comp1 = str.getListIndex(interp, 1).getString();
		auto comp2 = str.getListIndex(interp, 2).getString();
		if (StringOp::startsWith(comp2, "unicode")) {
			if (auto u = StringOp::stringToBase<10, unsigned>(comp2.substr(7))) {
				return parseKeyEvent(comp1, *u);
			}
		}
	}
	throw CommandException("Invalid keyboard event: ", str.getString());
}

[[nodiscard]] static bool upDown(std::string_view str)
{
	if (str == "up") {
		return true;
	} else if (str == "down") {
		return false;
	}
	throw CommandException(
		"Invalid direction (expected 'up' or 'down'): ", str);
}

[[nodiscard]] static Event parseMouseEvent(const TclObject& str, Interpreter& interp)
{
	auto len = str.getListLength(interp);
	if (len >= 2) {
		auto comp1 = str.getListIndex(interp, 1).getString();
		if (comp1 == "motion") {
			if (len == 2) {
				return Event::create<GroupEvent>(
					std::vector{EventType::MOUSE_MOTION},
					makeTclList("mouse", comp1));
			} else if (len == one_of(4u, 6u)) {
				int absX = 0, absY = 0;
				if (len == 6) {
					absX = str.getListIndex(interp, 4).getInt(interp);
					absY = str.getListIndex(interp, 5).getInt(interp);
				} else {
					// for bw-compat also allow events without absX,absY
				}
				return Event::create<MouseMotionEvent>(
					str.getListIndex(interp, 2).getInt(interp),
					str.getListIndex(interp, 3).getInt(interp),
					absX, absY);
			}
		} else if (StringOp::startsWith(comp1, "button")) {
			if (len == 2) {
				return Event::create<GroupEvent>(
					std::vector{EventType::MOUSE_BUTTON_UP, EventType::MOUSE_BUTTON_DOWN},
					makeTclList("mouse", "button"));
			} else if (len == 3) {
				if (auto button = StringOp::stringToBase<10, unsigned>(comp1.substr(6))) {
					if (upDown(str.getListIndex(interp, 2).getString())) {
						return Event::create<MouseButtonUpEvent>  (*button);
					} else {
						return Event::create<MouseButtonDownEvent>(*button);
					}
				}
			}
		} else if (comp1 == "wheel") {
			if (len == 2) {
				return Event::create<GroupEvent>(
					std::vector{EventType::MOUSE_WHEEL},
					makeTclList("mouse", comp1));
			} else if (len == 4) {
				return Event::create<MouseWheelEvent>(
					str.getListIndex(interp, 2).getInt(interp),
					str.getListIndex(interp, 3).getInt(interp));
			}
		}
	}
	throw CommandException("Invalid mouse event: ", str.getString());
}

[[nodiscard]] static Event parseOsdControlEvent(const TclObject& str, Interpreter& interp)
{
	if (str.getListLength(interp) == 3) {
		auto buttonName = str.getListIndex(interp, 1).getString();
		unsigned button = [&] {
			if (buttonName == "LEFT") {
				return OsdControlEvent::LEFT_BUTTON;
			} else if (buttonName == "RIGHT") {
				return OsdControlEvent::RIGHT_BUTTON;
			} else if (buttonName == "UP") {
				return OsdControlEvent::UP_BUTTON;
			} else if (buttonName == "DOWN") {
				return OsdControlEvent::DOWN_BUTTON;
			} else if (buttonName == "A") {
				return OsdControlEvent::A_BUTTON;
			} else if (buttonName == "B") {
				return OsdControlEvent::B_BUTTON;
			} else {
				throw CommandException(
					"Invalid OSDcontrol event, invalid button name: ",
					buttonName);
			}
		}();
		auto buttonAction = str.getListIndex(interp, 2).getString();
		if (buttonAction == "RELEASE") {
			return Event::create<OsdControlReleaseEvent>(button, Event{});
		} else if (buttonAction == "PRESS") {
			return Event::create<OsdControlPressEvent>  (button, Event{});
		}
	}
	throw CommandException("Invalid OSDcontrol event: ", str.getString());
}

[[nodiscard]] static Event parseJoystickEvent(const TclObject& str, Interpreter& interp)
{
	auto len = str.getListLength(interp);
	if (len >= 2) {
		auto comp0 = str.getListIndex(interp, 0).getString(); // joyN
		auto comp1 = str.getListIndex(interp, 1).getString();

		if (len == 2) {
			if (StringOp::startsWith(comp1, "button")) {
				return Event::create<GroupEvent>(
					std::vector{EventType::JOY_BUTTON_UP, EventType::JOY_BUTTON_DOWN},
					makeTclList("joy", "button"));
			} else if (StringOp::startsWith(comp1, "axis")) {
				return Event::create<GroupEvent>(
					std::vector{EventType::JOY_AXIS_MOTION},
					makeTclList("joy", "axis"));
			} else if (StringOp::startsWith(comp1, "hat")) {
				return Event::create<GroupEvent>(
					std::vector{EventType::JOY_HAT},
					makeTclList("joy", "hat"));
			}
		} else if (len == 3) {
			auto comp2 = str.getListIndex(interp, 2);
			if (auto j = StringOp::stringToBase<10, unsigned>(comp0.substr(3))) {
				unsigned joystick = *j - 1;
				if (StringOp::startsWith(comp1, "button")) {
					if (auto button = StringOp::stringToBase<10, unsigned>(comp1.substr(6))) {
						if (upDown(comp2.getString())) {
							return Event::create<JoystickButtonUpEvent>  (joystick, *button);
						} else {
							return Event::create<JoystickButtonDownEvent>(joystick, *button);
						}
					}
				} else if (StringOp::startsWith(comp1, "axis")) {
					if (auto axis = StringOp::stringToBase<10, unsigned>(comp1.substr(4))) {
						int value = str.getListIndex(interp, 2).getInt(interp);
						return Event::create<JoystickAxisMotionEvent>(joystick, *axis, value);
					}
				} else if (StringOp::startsWith(comp1, "hat")) {
					if (auto hat = StringOp::stringToBase<10, unsigned>(comp1.substr(3))) {
						auto valueStr = str.getListIndex(interp, 2).getString();
						int value = [&] {
							if      (valueStr == "up")        return SDL_HAT_UP;
							else if (valueStr == "right")     return SDL_HAT_RIGHT;
							else if (valueStr == "down")      return SDL_HAT_DOWN;
							else if (valueStr == "left")      return SDL_HAT_LEFT;
							else if (valueStr == "rightup")   return SDL_HAT_RIGHTUP;
							else if (valueStr == "rightdown") return SDL_HAT_RIGHTDOWN;
							else if (valueStr == "leftup")    return SDL_HAT_LEFTUP;
							else if (valueStr == "leftdown")  return SDL_HAT_LEFTDOWN;
							else if (valueStr == "center")    return SDL_HAT_CENTERED;
							else {
								throw CommandException("Invalid hat value: ", valueStr);
							}
						}();
						return Event::create<JoystickHatEvent>(joystick, *hat, value);
					}
				}
			}
		}
	}
	throw CommandException("Invalid joystick event: ", str.getString());
}

[[nodiscard]] static Event parseFocusEvent(const TclObject& str, Interpreter& interp)
{
	if (str.getListLength(interp) != 2) {
		throw CommandException("Invalid focus event: ", str.getString());
	}
	return Event::create<FocusEvent>(str.getListIndex(interp, 1).getBoolean(interp));
}

[[nodiscard]] static Event parseFileDropEvent(const TclObject& str, Interpreter& interp)
{
	if (str.getListLength(interp) != 1) {
		throw CommandException("Invalid filedrop event: ", str.getString());
	}
	return Event::create<GroupEvent>(
		std::vector{EventType::FILEDROP},
		makeTclList("filename"));
}

[[nodiscard]] static Event parseResizeEvent(const TclObject& str, Interpreter& interp)
{
	if (str.getListLength(interp) != 3) {
		throw CommandException("Invalid resize event: ", str.getString());
	}
	return Event::create<ResizeEvent>(
		str.getListIndex(interp, 1).getInt(interp),
		str.getListIndex(interp, 2).getInt(interp));
}

[[nodiscard]] static Event parseQuitEvent(const TclObject& str, Interpreter& interp)
{
	if (str.getListLength(interp) != 1) {
		throw CommandException("Invalid quit event: ", str.getString());
	}
	return Event::create<QuitEvent>();
}

Event createInputEvent(const TclObject& str, Interpreter& interp)
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
	} else if (type == "filedrop") {
		return parseFileDropEvent(str, interp);
	} else if (type == "resize") {
		return parseResizeEvent(str, interp);
	} else if (type == "quit") {
		return parseQuitEvent(str, interp);
	} else if (type == "command") {
		return Event::create<KeyUpEvent>(Keys::K_UNKNOWN); // dummy event, for bw compat
		//return parseCommandEvent(str);
	} else if (type == "OSDcontrol") {
		return parseOsdControlEvent(str, interp);
	} else {
		// fall back
		return parseKeyEvent(type, 0);
	}
}
Event createInputEvent(std::string_view str, Interpreter& interp)
{
	return createInputEvent(TclObject(str), interp);
}

} // namespace openmsx::InputEventFactory
