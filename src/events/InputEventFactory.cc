// $Id$

#include "InputEventFactory.hh"
#include "InputEvents.hh"
#include "CommandException.hh"
#include "StringOp.hh"
#include "Interpreter.hh"

using std::string;
using std::vector;

namespace openmsx {

namespace InputEventFactory {

static EventPtr parseKeyEvent(const string& str)
{
	Keys::KeyCode keyCode = Keys::getCode(str);
	if (keyCode == Keys::K_NONE) {
		throw CommandException("Invalid keycode: " + str);
	}
	if (keyCode & Keys::KD_RELEASE) {
		return EventPtr(new KeyUpEvent(keyCode));
	} else {
		return EventPtr(new KeyDownEvent(keyCode));
	}
}

static EventPtr parseKeyEvent(
		const string& str, const vector<string>& components)
{
	if (components.size() != 2) {
		throw CommandException("Invalid keyboard event: " + str);
	}
	return parseKeyEvent(components[1]);
}

static bool upDown(const string& str)
{
	if (str == "up") {
		return true;
	} else if (str == "down") {
		return false;
	} else {
		throw CommandException("Invalid direction (expected 'up' or 'down'): " + str);
	}
}

static EventPtr parseMouseEvent(
		const string& str, const vector<string>& components)
{
	if (components[1] == "motion") {
		if (components.size() != 4) {
			throw CommandException("Invalid mouse motion event: " + str);
		}
		return EventPtr(new MouseMotionEvent(
			StringOp::stringToInt(components[2]),
			StringOp::stringToInt(components[3])));
	} else if (StringOp::startsWith(components[1], "button")) {
		if (components.size() != 3) {
			throw CommandException("Invalid mouse button event: " + str);
		}
		unsigned button = StringOp::stringToInt(components[1].substr(6));
		if (upDown(components[2])) {
			return EventPtr(new MouseButtonUpEvent(button));
		} else {
			return EventPtr(new MouseButtonDownEvent(button));
		}
	} else {
		throw CommandException("Invalid mouse event: " + str);
	}
}

static EventPtr parseJoystickEvent(
		const string& str, const vector<string>& components)
{
	if (components.size() != 3) {
		throw CommandException("Invalid joystick event: " + str);
	}
	unsigned joystick = StringOp::stringToInt(components[0].substr(3));
	if (StringOp::startsWith(components[1], "button")) {
		unsigned button = StringOp::stringToInt(components[1].substr(6));
		if (upDown(components[2])) {
			return EventPtr(new JoystickButtonUpEvent(joystick, button));
		} else {
			return EventPtr(new JoystickButtonDownEvent(joystick, button));
		}
	} else if (StringOp::startsWith(components[1], "axis")) {
		unsigned axis = StringOp::stringToInt(components[1].substr(4));
		short value = StringOp::stringToInt(components[2]);
		return EventPtr(new JoystickAxisMotionEvent(joystick, axis, value));
	} else {
		throw CommandException("Invalid joystick event: " + str);
	}
}

static EventPtr parseFocusEvent(
		const string& str, const vector<string>& components)
{
	if (components.size() != 2) {
		throw CommandException("Invalid focus event: " + str);
	}
	bool gain = StringOp::stringToBool(components[1]);
	return EventPtr(new FocusEvent(gain));
}

static EventPtr parseResizeEvent(
		const string& str, const vector<string>& components)
{
	if (components.size() != 3) {
		throw CommandException("Invalid resize event: " + str);
	}
	int x = StringOp::stringToInt(components[1]);
	int y = StringOp::stringToInt(components[2]);
	return EventPtr(new ResizeEvent(x, y));
}

static EventPtr parseQuitEvent(
		const string& str, const vector<string>& components)
{
	if (components.size() != 1) {
		throw CommandException("Invalid resize event: " + str);
	}
	return EventPtr(new QuitEvent());
}

static EventPtr parseCommandEvent(
		const string& str, const vector<string>& components)
{
	if (components.size() < 2) {
		throw CommandException("Invalid command event: " + str);
	}
	vector<string> tokens(components.begin() + 1, components.end());
	return EventPtr(new MSXCommandEvent(tokens));
}

EventPtr createInputEvent(const string& str)
{
	vector<string> components;
	Interpreter::splitList(str, components, 0);
	if (components.empty()) {
		throw CommandException("Invalid event: \"" + str + "\"");
	}
	if (components[0] == "keyb") {
		return parseKeyEvent(str, components);
	} else if (components[0] == "mouse") {
		return parseMouseEvent(str, components);
	} else if (StringOp::startsWith(components[0], "joy")) {
		return parseJoystickEvent(str, components);
	} else if (components[0] == "focus") {
		return parseFocusEvent(str, components);
	} else if (components[0] == "resize") {
		return parseResizeEvent(str, components);
	} else if (components[0] == "quit") {
		return parseQuitEvent(str, components);
	} else if (components[0] == "command") {
		return parseCommandEvent(str, components);
	} else {
		// fall back
		return parseKeyEvent(components[0]);
	}
}

} // namespace InputEventFactory

} // namespace openmsx
