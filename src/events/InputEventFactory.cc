#include "InputEventFactory.hh"

#include "Event.hh"
#include "SDLKey.hh"

#include "CommandException.hh"
#include "TclObject.hh"

#include "StringOp.hh"
#include "one_of.hh"

#include <SDL.h>

namespace openmsx::InputEventFactory {

[[nodiscard]] static Event parseKeyEvent(std::string_view str, uint32_t unicode)
{
	auto key = SDLKey::fromString(str);
	if (!key) {
		throw CommandException("Invalid keycode: ", str);
	}

	SDL_Event evt;
	evt.key = SDL_KeyboardEvent{};
	auto& e = evt.key;

	e.timestamp = SDL_GetTicks();
	e.keysym = key->sym;
	e.keysym.unused = unicode;
	if (key->down) {
		e.type = SDL_KEYDOWN;
		e.state = SDL_PRESSED;
		return KeyDownEvent(evt);
	} else {
		e.type = SDL_KEYUP;
		e.state = SDL_RELEASED;
		return KeyUpEvent(evt);
	}
}

[[nodiscard]] static Event parseKeyEvent(const TclObject& str, Interpreter& interp)
{
	auto len = str.getListLength(interp);
	if (len == 1) {
		return GroupEvent(
			std::initializer_list<EventType>{EventType::KEY_UP, EventType::KEY_DOWN},
			makeTclList("keyb"));
	} else if (len == 2) {
		auto comp1 = str.getListIndex(interp, 1).getString();
		return parseKeyEvent(comp1, 0);
	} else if (len == 3) {
		auto comp1 = str.getListIndex(interp, 1).getString();
		auto comp2 = str.getListIndex(interp, 2).getString();
		if (comp2.starts_with("unicode")) {
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
				return GroupEvent(
					std::initializer_list<EventType>{EventType::MOUSE_MOTION},
					makeTclList("mouse", comp1));
			} else if (len == one_of(4u, 6u)) {
				int absX = 0, absY = 0;
				if (len == 6) {
					absX = str.getListIndex(interp, 4).getInt(interp);
					absY = str.getListIndex(interp, 5).getInt(interp);
				} else {
					// for bw-compat also allow events without absX,absY
				}

				SDL_Event evt;
				evt.motion = SDL_MouseMotionEvent{};
				auto& e = evt.motion;

				e.type = SDL_MOUSEMOTION;
				e.timestamp = SDL_GetTicks();
				e.x = absX;
				e.y = absY;
				e.xrel = str.getListIndex(interp, 2).getInt(interp);
				e.yrel = str.getListIndex(interp, 3).getInt(interp);
				return MouseMotionEvent(evt);
			}
		} else if (comp1.starts_with("button")) {
			if (len == 2) {
				return GroupEvent(
					std::initializer_list<EventType>{EventType::MOUSE_BUTTON_UP, EventType::MOUSE_BUTTON_DOWN},
					makeTclList("mouse", "button"));
			} else if (len == 3) {
				if (auto button = StringOp::stringToBase<10, unsigned>(comp1.substr(6))) {
					SDL_Event evt;
					evt.button = SDL_MouseButtonEvent{};
					auto& e = evt.button;

					e.timestamp = SDL_GetTicks();
					e.button = narrow<uint8_t>(*button);
					if (upDown(str.getListIndex(interp, 2).getString())) {
						e.type = SDL_MOUSEBUTTONUP;
						e.state = SDL_RELEASED;
						return MouseButtonUpEvent(evt);
					} else {
						e.type = SDL_MOUSEBUTTONDOWN;
						e.state = SDL_PRESSED;
						return MouseButtonDownEvent(evt);
					}
				}
			}
		} else if (comp1 == "wheel") {
			if (len == 2) {
				return GroupEvent(
					std::initializer_list<EventType>{EventType::MOUSE_WHEEL},
					makeTclList("mouse", comp1));
			} else if (len == 4) {
				SDL_Event evt;
				evt.wheel = SDL_MouseWheelEvent{};
				auto& e = evt.wheel;

				e.type = SDL_MOUSEWHEEL;
				e.timestamp = SDL_GetTicks();
				e.direction = SDL_MOUSEWHEEL_NORMAL;
				e.x = str.getListIndex(interp, 2).getInt(interp);
				e.y = str.getListIndex(interp, 3).getInt(interp);
				#if (SDL_VERSION_ATLEAST(2, 0, 18))
					e.preciseX = narrow_cast<float>(e.x);
					e.preciseY = narrow_cast<float>(e.y);
				#endif
				return MouseWheelEvent(evt);
			}
		}
	}
	throw CommandException("Invalid mouse event: ", str.getString());
}

[[nodiscard]] static Event parseOsdControlEvent(const TclObject& str, Interpreter& interp)
{
	if (str.getListLength(interp) == 3) {
		auto buttonName = str.getListIndex(interp, 1).getString();
		auto button = [&] {
			using enum OsdControlEvent::Button;
			if (buttonName == "LEFT") {
				return LEFT;
			} else if (buttonName == "RIGHT") {
				return RIGHT;
			} else if (buttonName == "UP") {
				return UP;
			} else if (buttonName == "DOWN") {
				return DOWN;
			} else if (buttonName == "A") {
				return A;
			} else if (buttonName == "B") {
				return B;
			} else {
				throw CommandException(
					"Invalid OSDcontrol event, invalid button name: ",
					buttonName);
			}
		}();
		auto buttonAction = str.getListIndex(interp, 2).getString();
		if (buttonAction == "RELEASE") {
			return OsdControlReleaseEvent(button);
		} else if (buttonAction == "PRESS") {
			return OsdControlPressEvent(button);
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
			using enum EventType;
			if (comp1.starts_with("button")) {
				return GroupEvent(
					std::initializer_list<EventType>{JOY_BUTTON_UP, JOY_BUTTON_DOWN},
					makeTclList("joy", "button"));
			} else if (comp1.starts_with("axis")) {
				return GroupEvent(
					std::initializer_list<EventType>{JOY_AXIS_MOTION},
					makeTclList("joy", "axis"));
			} else if (comp1.starts_with("hat")) {
				return GroupEvent(
					std::initializer_list<EventType>{JOY_HAT},
					makeTclList("joy", "hat"));
			}
		} else if (len == 3) {
			auto comp2 = str.getListIndex(interp, 2);
			if (auto j = StringOp::stringToBase<10, unsigned>(comp0.substr(3))) {
				unsigned joystick = *j - 1;
				if (comp1.starts_with("button")) {
					if (auto button = StringOp::stringToBase<10, unsigned>(comp1.substr(6))) {
						SDL_Event evt;
						evt.jbutton = SDL_JoyButtonEvent{};
						auto& e = evt.jbutton;

						e.timestamp = SDL_GetTicks();
						e.which = joystick;
						e.button = narrow_cast<uint8_t>(*button);
						if (upDown(comp2.getString())) {
							e.type = SDL_JOYBUTTONUP;
							e.state = SDL_RELEASED;
							return JoystickButtonUpEvent(evt);
						} else {
							e.type = SDL_JOYBUTTONDOWN;
							e.state = SDL_PRESSED;
							return JoystickButtonDownEvent(evt);
						}
					}
				} else if (comp1.starts_with("axis")) {
					if (auto axis = StringOp::stringToBase<10, unsigned>(comp1.substr(4))) {
						SDL_Event evt;
						evt.jaxis = SDL_JoyAxisEvent{};
						auto& e = evt.jaxis;

						e.type = SDL_JOYAXISMOTION;
						e.timestamp = SDL_GetTicks();
						e.which = joystick;
						e.axis = narrow_cast<uint8_t>(*axis);
						e.value = narrow_cast<int16_t>(str.getListIndex(interp, 2).getInt(interp));
						return JoystickAxisMotionEvent(evt);
					}
				} else if (comp1.starts_with("hat")) {
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
						SDL_Event evt;
						evt.jhat = SDL_JoyHatEvent{};
						auto& e = evt.jhat;

						e.type = SDL_JOYHATMOTION;
						e.timestamp = SDL_GetTicks();
						e.which = joystick;
						e.hat = narrow_cast<uint8_t>(*hat);
						e.value = narrow_cast<uint8_t>(value);
						return JoystickHatEvent(evt);
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
	bool gained = str.getListIndex(interp, 1).getBoolean(interp);

	SDL_Event evt;
	evt.window = SDL_WindowEvent{};
	auto& e = evt.window;

	e.type = SDL_WINDOWEVENT;
	e.timestamp = SDL_GetTicks();
	e.windowID = WindowEvent::getMainWindowId();
	e.event = gained ? SDL_WINDOWEVENT_FOCUS_GAINED : SDL_WINDOWEVENT_FOCUS_LOST;
	return WindowEvent(evt);
}

[[nodiscard]] static Event parseFileDropEvent(const TclObject& str, Interpreter& interp)
{
	if (str.getListLength(interp) != 1) {
		throw CommandException("Invalid filedrop event: ", str.getString());
	}
	return GroupEvent(
		std::initializer_list<EventType>{EventType::FILE_DROP},
		makeTclList("filename"));
}

[[nodiscard]] static Event parseQuitEvent(const TclObject& str, Interpreter& interp)
{
	if (str.getListLength(interp) != 1) {
		throw CommandException("Invalid quit event: ", str.getString());
	}
	return QuitEvent();
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
	} else if (type.starts_with("joy")) {
		return parseJoystickEvent(str, interp);
	} else if (type == "focus") {
		return parseFocusEvent(str, interp);
	} else if (type == "filedrop") {
		return parseFileDropEvent(str, interp);
	} else if (type == "quit") {
		return parseQuitEvent(str, interp);
	} else if (type == "command") {
		SDL_Event evt;
		evt.key = SDL_KeyboardEvent{};
		evt.key.type = SDL_KEYUP;
		evt.key.state = SDL_RELEASED;
		return KeyUpEvent(evt); // dummy event, for bw compat
		//return parseCommandEvent(str);
	} else if (type == "OSDcontrol") {
		return parseOsdControlEvent(str, interp);
	} else {
		// fall back
		return parseKeyEvent(str.getString(), 0);
	}
}
Event createInputEvent(std::string_view str, Interpreter& interp)
{
	return createInputEvent(TclObject(str), interp);
}

} // namespace openmsx::InputEventFactory
