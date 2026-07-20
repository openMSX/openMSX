#include "catch.hpp"
#include "AnalogInput.hh"

#include <SDL.h>

using namespace openmsx;

TEST_CASE("AnalogInput: toString, parse")
{
	auto compare = [](const AnalogInput& input, const std::string& str) {
		auto str2 = toString(input);
		CHECK(str2 == str);

		auto input2 = parseAnalogInput(str);
		CHECK(input2);
		CHECK(*input2 == input);
	};
	using ID = JoystickId;

	SECTION("mouse axis") {
		compare(AnalogMouseAxis(0), "mouse X-axis");
		compare(AnalogMouseAxis(1), "mouse Y-axis");
		// alternative spelling
		CHECK(parseAnalogInput("mouse x-axis") == AnalogMouseAxis(0));
		CHECK(parseAnalogInput("mouse y-axis") == AnalogMouseAxis(1));
	}
	SECTION("joystick axis") {
		compare(AnalogJoystickAxis(ID(0), 1), "joy1 axis1");
		compare(AnalogJoystickAxis(ID(3), 2), "joy4 axis2");
	}
	SECTION("parse error") {
		CHECK(!parseAnalogInput("")); // no type
		CHECK(!parseAnalogInput("bla")); // invalid type

		CHECK(!parseAnalogInput("mouse")); // missing name
		CHECK(!parseAnalogInput("mouse invalid-key"));
		CHECK(!parseAnalogInput("mouse A-axis"));
		CHECK(!parseAnalogInput("mouse X-axis too-much"));

		CHECK(!parseAnalogInput("joyBLA")); // no number
		CHECK(!parseAnalogInput("joy2")); // missing axis
		CHECK(!parseAnalogInput("joy0 axis3")); // joy 0
		CHECK(!parseAnalogInput("joy2 bla")); // no axis
		CHECK(!parseAnalogInput("joy2 axisXYZ")); // no valid axis
	}
}

TEST_CASE("AnalogInput: capture")
{
	auto getJoyDeadZone = [](JoystickId /*joystick*/) { return 25; };
	auto check = [&](const Event& event, std::string_view expected) {
		auto input = captureAnalogInput(event, getJoyDeadZone);
		if (expected.empty()) {
			CHECK(!input);
		} else {
			CHECK(toString(*input) == expected);
		}
	};

	SDL_Event sdl = {};

	SECTION("mouse") {
		sdl.motion = SDL_MouseMotionEvent{};
		sdl.motion.type = SDL_MOUSEMOTION;
		sdl.motion.xrel = 0;
		sdl.motion.yrel = 0;
		sdl.motion.x = 30;
		sdl.motion.y = 40;
		check(MouseMotionEvent(sdl), "");
		sdl.motion.xrel = 10;
		sdl.motion.yrel = 2;
		check(MouseMotionEvent(sdl), "mouse X-axis");
		sdl.motion.xrel = 3;
		sdl.motion.yrel = -9;
		check(MouseMotionEvent(sdl), "mouse Y-axis");
	}
	SECTION("joystick") {
		sdl.jaxis = SDL_JoyAxisEvent{};
		sdl.jaxis.type = SDL_JOYAXISMOTION;
		sdl.jaxis.which = 2;
		sdl.jaxis.axis = 1;
		sdl.jaxis.value = 32000;
		check(JoystickAxisMotionEvent(sdl), "joy3 axis1");
		sdl.jaxis.value = -32000;
		check(JoystickAxisMotionEvent(sdl), "joy3 axis1");
		sdl.jaxis.value = 20; // close to center
		check(JoystickAxisMotionEvent(sdl), "");
	}
}

TEST_CASE("AnalogInput: match")
{
	// define various events
	SDL_Event sdl;

	sdl.motion = SDL_MouseMotionEvent{};
	sdl.motion.type = SDL_MOUSEMOTION;
	sdl.motion.xrel = 0;
	sdl.motion.yrel = 1; // small movement
	sdl.motion.x = 30;
	sdl.motion.y = 40;
	auto mouseCenter = MouseMotionEvent(sdl);
	sdl.motion.xrel = 0;
	sdl.motion.yrel = -15;
	auto mouseUp = MouseMotionEvent(sdl);
	sdl.motion.xrel = 0;
	sdl.motion.yrel = 8;
	auto mouseDown = MouseMotionEvent(sdl);
	sdl.motion.xrel = -9;
	sdl.motion.yrel = 0;
	auto mouseLeft = MouseMotionEvent(sdl);
	sdl.motion.xrel = 12;
	sdl.motion.yrel = 0;
	auto mouseRight = MouseMotionEvent(sdl);

	sdl.jaxis = SDL_JoyAxisEvent{};
	sdl.jaxis.type = SDL_JOYAXISMOTION;
	sdl.jaxis.which = 0;
	sdl.jaxis.axis = 1;
	sdl.jaxis.value = 1; // not fully center
	auto joyCenter = JoystickAxisMotionEvent(sdl);
	sdl.jaxis.axis = 2;
	sdl.jaxis.value = -20123;
	auto joyUp = JoystickAxisMotionEvent(sdl);
	sdl.jaxis.axis = 2;
	sdl.jaxis.value = 30789;
	auto joyDown = JoystickAxisMotionEvent(sdl);
	sdl.jaxis.axis = 1;
	sdl.jaxis.value = -30001;
	auto joyLeft = JoystickAxisMotionEvent(sdl);
	sdl.jaxis.axis = 1;
	sdl.jaxis.value = 20222;
	auto joyRight = JoystickAxisMotionEvent(sdl);

	// check against various BooleanInputs
	auto getJoyDeadZone = [](JoystickId /*joystick*/) { return 25; };
	auto check = [&](const std::optional<AnalogInput>& binding, const Event& event, std::optional<int> expected) {
		REQUIRE(binding);
		CHECK(match(*binding, event, getJoyDeadZone) == expected);
	};

	auto bMouseXaxis = parseAnalogInput("mouse X-axis");
	check(bMouseXaxis, mouseCenter, 0);
	check(bMouseXaxis, mouseUp, 0);
	check(bMouseXaxis, mouseDown, 0);
	check(bMouseXaxis, mouseLeft, -9);
	check(bMouseXaxis, mouseRight, 12);
	check(bMouseXaxis, joyCenter, {});
	check(bMouseXaxis, joyUp, {});
	check(bMouseXaxis, joyDown, {});
	check(bMouseXaxis, joyLeft, {});
	check(bMouseXaxis, joyRight, {});

	auto bMouseYaxis = parseAnalogInput("mouse Y-axis");
	check(bMouseYaxis, mouseCenter, 0);
	check(bMouseYaxis, mouseUp, -15);
	check(bMouseYaxis, mouseDown, 8);
	check(bMouseYaxis, mouseLeft, 0);
	check(bMouseYaxis, mouseRight, 0);
	check(bMouseYaxis, joyCenter, {});
	check(bMouseYaxis, joyUp, {});
	check(bMouseYaxis, joyDown, {});
	check(bMouseYaxis, joyLeft, {});
	check(bMouseYaxis, joyRight, {});

	auto bJoy1Axis1 = parseAnalogInput("joy1 axis1");
	check(bJoy1Axis1, mouseCenter, {});
	check(bJoy1Axis1, mouseUp, {});
	check(bJoy1Axis1, mouseDown, {});
	check(bJoy1Axis1, mouseLeft, {});
	check(bJoy1Axis1, mouseRight, {});
	check(bJoy1Axis1, joyCenter, 0);
	check(bJoy1Axis1, joyUp, {});
	check(bJoy1Axis1, joyDown, {});
	check(bJoy1Axis1, joyLeft, -30001);
	check(bJoy1Axis1, joyRight, 20222);

	auto bJoy1Axis2 = parseAnalogInput("joy1 axis2");
	check(bJoy1Axis2, mouseCenter, {});
	check(bJoy1Axis2, mouseUp, {});
	check(bJoy1Axis2, mouseDown, {});
	check(bJoy1Axis2, mouseLeft, {});
	check(bJoy1Axis2, mouseRight, {});
	check(bJoy1Axis2, joyCenter, {});
	check(bJoy1Axis2, joyUp, -20123);
	check(bJoy1Axis2, joyDown, 30789);
	check(bJoy1Axis2, joyLeft, {});
	check(bJoy1Axis2, joyRight, {});
}
