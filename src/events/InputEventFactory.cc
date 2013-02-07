// $Id$

#include "InputEventFactory.hh"
#include "InputEvents.hh"
#include "CommandException.hh"
#include "StringOp.hh"
#include "Interpreter.hh"
#include "openmsx.hh"

using std::string;
using std::vector;
using std::make_shared;

namespace openmsx {

namespace InputEventFactory {

static EventPtr parseKeyEvent(const string& str, const int unicode)
{
	Keys::KeyCode keyCode = Keys::getCode(str);
	if (keyCode == Keys::K_NONE) {
		throw CommandException("Invalid keycode: " + str);
	}
	if (keyCode & Keys::KD_RELEASE) {
		return make_shared<KeyUpEvent  >(keyCode, unicode);
	} else {
		return make_shared<KeyDownEvent>(keyCode, unicode);
	}
}

static EventPtr parseKeyEvent(
		const string& str, const vector<string>& components)
{
	if (components.size() == 2) {
		return parseKeyEvent(components[1], 0);
	} else if ((components.size() == 3) &&
	           (StringOp::startsWith(components[2], "unicode"))) {
		return parseKeyEvent(components[1],
		                     stoi(string_ref(components[2]).substr(7)));
	} else {
		throw CommandException("Invalid keyboard event: " + str);
	}
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
	if (components.size() < 2) {
		throw CommandException("Invalid mouse event: " + str);
	}
	if (components[1] == "motion") {
		if (components.size() != 4) {
			throw CommandException("Invalid mouse motion event: " + str);
		}
		return make_shared<MouseMotionEvent>(
			StringOp::stringToInt(components[2]),
			StringOp::stringToInt(components[3]));
	} else if (StringOp::startsWith(components[1], "button")) {
		if (components.size() != 3) {
			throw CommandException("Invalid mouse button event: " + str);
		}
		unsigned button = stoi(string_ref(components[1]).substr(6));
		if (upDown(components[2])) {
			return make_shared<MouseButtonUpEvent>  (button);
		} else {
			return make_shared<MouseButtonDownEvent>(button);
		}
	} else {
		throw CommandException("Invalid mouse event: " + str);
	}
}

static EventPtr parseOsdControlEvent(
		const string& str, const vector<string>& components)
{
#ifdef DEBUG
	ad_printf("components.size(): %d\n", components.size());
	for (unsigned cnt=0; cnt != components.size(); cnt++) {
		ad_printf("component[%d]: %s\n", cnt, components[cnt].c_str());
	}
#endif
	if (components.size() != 3) {
		throw CommandException("Invalid OSDcontrol event: " + str);
	}
	string buttonName = components[1];
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
		throw CommandException("Invalid OSDcontrol event: " + str);
	}
	if (components[2] == "RELEASE") {
		return make_shared<OsdControlReleaseEvent>(button, nullptr);
	} else if (components[2] == "PRESS") {
		return make_shared<OsdControlPressEvent>(button, nullptr);
	}
	else {
		throw CommandException("Invalid OSDcontrol event: " + str);
	}
}

static EventPtr parseJoystickEvent(
		const string& str, const vector<string>& components)
{
	if (components.size() != 3) {
		throw CommandException("Invalid joystick event: " + str);
	}
	int joystick;
	string joyString = components[0].substr(3);
	if (!StringOp::stringToInt(joyString, joystick) || (joystick == 0)) {
		throw CommandException("Invalid joystick number: " + joyString);
	}
	--joystick;

	if (StringOp::startsWith(components[1], "button")) {
		int button;
		string joyButtonString = components[1].substr(6);
		if (!StringOp::stringToInt(joyButtonString, button)) {
			throw CommandException("Invalid joystick button number: " + joyButtonString);
		}
		if (upDown(components[2])) {
			return make_shared<JoystickButtonUpEvent>(joystick, button);
		} else {
			return make_shared<JoystickButtonDownEvent>(joystick, button);
		}
	} else if (StringOp::startsWith(components[1], "axis")) {
		int axis;
		string axisString = components[1].substr(4);
		if (!StringOp::stringToInt(axisString, axis)) {
			throw CommandException("Invalid axis number: " + axisString);
		}
		int value;
		if (!StringOp::stringToInt(components[2], value) || (short(value) != value)) {
			throw CommandException("Invalid value: " + components[2]);
		}
		return make_shared<JoystickAxisMotionEvent>(joystick, axis, value);
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
	return make_shared<FocusEvent>(gain);
}

static EventPtr parseResizeEvent(
		const string& str, const vector<string>& components)
{
	if (components.size() != 3) {
		throw CommandException("Invalid resize event: " + str);
	}
	int x = StringOp::stringToInt(components[1]);
	int y = StringOp::stringToInt(components[2]);
	return make_shared<ResizeEvent>(x, y);
}

static EventPtr parseQuitEvent(
		const string& str, const vector<string>& components)
{
	if (components.size() != 1) {
		throw CommandException("Invalid quit event: " + str);
	}
	return make_shared<QuitEvent>();
}

EventPtr createInputEvent(const string& str)
{
	auto components = Interpreter::splitList(str, nullptr);
	if (components.empty()) {
		throw CommandException("Invalid event: \"" + str + '\"');
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
		return EventPtr();
		//return parseCommandEvent(str, components);
	} else if (components[0] == "OSDcontrol") {
		return parseOsdControlEvent(str, components);
	} else {
		// fall back
		return parseKeyEvent(components[0], 0);
	}
}

} // namespace InputEventFactory

} // namespace openmsx
