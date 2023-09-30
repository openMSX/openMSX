#include "catch.hpp"
#include "BooleanInput.hh"

#include "SDL.h"

using namespace openmsx;

TEST_CASE("BooleanInput: toString, parse")
{
	auto compare = [](const BooleanInput& input, const std::string& str) {
		auto str2 = toString(input);
		CHECK(str2 == str);

		auto input2 = parseBooleanInput(str);
		CHECK(input2);
		CHECK(*input2 == input);
	};

	SECTION("keyboard") {
		compare(BooleanKeyboard(SDLK_a), "keyb A");
		compare(BooleanKeyboard(SDLK_LALT), "keyb Left Alt");

		// old key names
		auto k1 = parseBooleanInput("keyb PrintSCreen");
		REQUIRE(k1);
		auto k2 = parseBooleanInput("keyb PRINT");
		REQUIRE(k2);
		CHECK(*k1 == *k2);
		CHECK(*k2 == BooleanKeyboard(SDLK_PRINTSCREEN));
		CHECK(toString(*k2) == "keyb PrintScreen");
	}
	SECTION("mouse button") {
		compare(BooleanMouseButton( 2), "mouse button2");
		compare(BooleanMouseButton(11), "mouse button11");
	}
	SECTION("joystick button") {
		compare(BooleanJoystickButton(0, 2), "joy1 button2");
		compare(BooleanJoystickButton(1, 4), "joy2 button4");
	}
	SECTION("joystick hat") {
		compare(BooleanJoystickHat(0, 1, BooleanJoystickHat::UP),    "joy1 hat1 up");
		compare(BooleanJoystickHat(3, 5, BooleanJoystickHat::DOWN),  "joy4 hat5 down");
		compare(BooleanJoystickHat(2, 4, BooleanJoystickHat::LEFT),  "joy3 hat4 left");
		compare(BooleanJoystickHat(1, 2, BooleanJoystickHat::RIGHT), "joy2 hat2 right");
	}
	SECTION("joystick axis") {
		compare(BooleanJoystickAxis(0, 1, BooleanJoystickAxis::POS), "joy1 +axis1");
		compare(BooleanJoystickAxis(3, 2, BooleanJoystickAxis::NEG), "joy4 -axis2");
	}
	SECTION("parse error") {
		CHECK(!parseBooleanInput("")); // no type
		CHECK(!parseBooleanInput("bla")); // invalid type

		CHECK(!parseBooleanInput("keyb")); // missing name
		CHECK(!parseBooleanInput("keyb invalid-key"));
		CHECK(!parseBooleanInput("keyb A too-many-args"));
		CHECK( parseBooleanInput("keyb B")); // OK

		CHECK(!parseBooleanInput("mouse")); // missing button
		CHECK(!parseBooleanInput("mouse buttonA")); // invalid number
		CHECK(!parseBooleanInput("mouse button1 too-many-args"));
		CHECK( parseBooleanInput("mouse button1")); // OK

		CHECK(!parseBooleanInput("joy")); // missing joystick-number
		CHECK(!parseBooleanInput("joyZ")); // invalid number
		CHECK(!parseBooleanInput("joy1")); // missing subtype
		CHECK(!parseBooleanInput("joy2 button")); // missing button number
		CHECK(!parseBooleanInput("joy3 buttonA")); // invalid number
		CHECK(!parseBooleanInput("joy4 button2 too-many-args"));
		CHECK( parseBooleanInput("joy4 button2")); // OK

		CHECK(!parseBooleanInput("joy2 hat")); // missing hat number
		CHECK(!parseBooleanInput("joy2 hatA")); // invalid number
		CHECK(!parseBooleanInput("joy2 hat3")); // missing value
		CHECK(!parseBooleanInput("joy2 hat3 bla")); // invalid value
		CHECK(!parseBooleanInput("joy2 hat3 up too-many-args"));
		CHECK( parseBooleanInput("joy2 hat3 up")); // OK

		CHECK(!parseBooleanInput("joy2 axis3")); // missing +/-
		CHECK(!parseBooleanInput("joy2 +axis")); // missing number
		CHECK(!parseBooleanInput("joy2 +axis@")); // invalid number
		CHECK( parseBooleanInput("joy2 +axis2")); // OK
	}
}

TEST_CASE("BooleanInput: capture")
{
	auto getJoyDeadZone = [](int /*joystick*/) { return 25; };
	auto check = [&](const Event& event, const std::string& expected) {
		auto input = captureBooleanInput(event, getJoyDeadZone);
		if (expected.empty()) {
			CHECK(!input);
		} else {
			CHECK(toString(*input) == expected);
		}
	};

	SDL_Event sdl = {};

	SECTION("keyboard") {
		check(KeyDownEvent::create(SDLK_h), "keyb H");
		check(KeyUpEvent  ::create(SDLK_h), "");
	}
	SECTION("mouse") {
		sdl.type = SDL_MOUSEBUTTONDOWN;
		sdl.button.button = 2;
		check(MouseButtonDownEvent(sdl), "mouse button2");
		sdl.type = SDL_MOUSEBUTTONUP;
		check(MouseButtonUpEvent(sdl), "");

		sdl.type = SDL_MOUSEMOTION;
		sdl.motion.xrel = 10;
		sdl.motion.yrel = 20;
		sdl.motion.x = 30;
		sdl.motion.y = 40;
		check(MouseMotionEvent(sdl), "");

		sdl.type = SDL_MOUSEWHEEL;
		sdl.wheel.x = 2;
		sdl.wheel.y = 4;
		check(MouseWheelEvent(sdl), "");
	}
	SECTION("joystick") {
		sdl.type = SDL_JOYBUTTONDOWN;
		sdl.jbutton.which = 0;
		sdl.jbutton.button = 3;
		check(JoystickButtonDownEvent(sdl), "joy1 button3");
		sdl.type = SDL_JOYBUTTONUP;
		check(JoystickButtonUpEvent(sdl), "");

		sdl.type = SDL_JOYHATMOTION;
		sdl.jhat.which = 1;
		sdl.jhat.hat = 1;
		sdl.jhat.value = SDL_HAT_LEFT;
		check(JoystickHatEvent(sdl), "joy2 hat1 left");
		sdl.jhat.value = SDL_HAT_CENTERED;
		check(JoystickHatEvent(sdl), "");
		sdl.jhat.value = SDL_HAT_LEFTDOWN;
		check(JoystickHatEvent(sdl), "");

		sdl.type = SDL_JOYAXISMOTION;
		sdl.jaxis.which = 2;
		sdl.jaxis.axis = 1;
		sdl.jaxis.value = 32000;
		check(JoystickAxisMotionEvent(sdl), "joy3 +axis1");
		sdl.jaxis.value = -32000;
		check(JoystickAxisMotionEvent(sdl), "joy3 -axis1");
		sdl.jaxis.value = 2000; // close to center
		check(JoystickAxisMotionEvent(sdl), "");
	}
	SECTION("other SDL") {
		sdl.type = SDL_WINDOWEVENT;
		check(WindowEvent(sdl), "");
	}
	SECTION("other") {
		check(FileDropEvent("file.ext"), "");
		check(BootEvent{}, "");
	}
}
