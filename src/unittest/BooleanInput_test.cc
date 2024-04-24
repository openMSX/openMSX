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
	using ID = JoystickId;

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
		compare(BooleanJoystickButton(ID(0), 2), "joy1 button2");
		compare(BooleanJoystickButton(ID(1), 4), "joy2 button4");
	}
	SECTION("joystick hat") {
		compare(BooleanJoystickHat(ID(0), 1, BooleanJoystickHat::Direction::UP),    "joy1 hat1 up");
		compare(BooleanJoystickHat(ID(3), 5, BooleanJoystickHat::Direction::DOWN),  "joy4 hat5 down");
		compare(BooleanJoystickHat(ID(2), 4, BooleanJoystickHat::Direction::LEFT),  "joy3 hat4 left");
		compare(BooleanJoystickHat(ID(1), 2, BooleanJoystickHat::Direction::RIGHT), "joy2 hat2 right");
	}
	SECTION("joystick axis") {
		compare(BooleanJoystickAxis(ID(0), 1, BooleanJoystickAxis::Direction::POS), "joy1 +axis1");
		compare(BooleanJoystickAxis(ID(3), 2, BooleanJoystickAxis::Direction::NEG), "joy4 -axis2");
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
	auto getJoyDeadZone = [](JoystickId /*joystick*/) { return 25; };
	auto check = [&](const Event& event, std::string_view expected) {
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
		sdl.button = SDL_MouseButtonEvent{};
		sdl.button.type = SDL_MOUSEBUTTONDOWN;
		sdl.button.button = 2;
		check(MouseButtonDownEvent(sdl), "mouse button2");
		sdl.button.type = SDL_MOUSEBUTTONUP;
		check(MouseButtonUpEvent(sdl), "");

		sdl.motion = SDL_MouseMotionEvent{};
		sdl.motion.type = SDL_MOUSEMOTION;
		sdl.motion.xrel = 10;
		sdl.motion.yrel = 20;
		sdl.motion.x = 30;
		sdl.motion.y = 40;
		check(MouseMotionEvent(sdl), "");

		sdl.wheel = SDL_MouseWheelEvent{};
		sdl.wheel.type = SDL_MOUSEWHEEL;
		sdl.wheel.x = 2;
		sdl.wheel.y = 4;
		check(MouseWheelEvent(sdl), "");
	}
	SECTION("joystick") {
		sdl.jbutton = SDL_JoyButtonEvent{};
		sdl.jbutton.type = SDL_JOYBUTTONDOWN;
		sdl.jbutton.which = 0;
		sdl.jbutton.button = 3;
		check(JoystickButtonDownEvent(sdl), "joy1 button3");
		sdl.jbutton.type = SDL_JOYBUTTONUP;
		check(JoystickButtonUpEvent(sdl), "");

		sdl.jhat = SDL_JoyHatEvent{};
		sdl.jhat.type = SDL_JOYHATMOTION;
		sdl.jhat.which = 1;
		sdl.jhat.hat = 1;
		sdl.jhat.value = SDL_HAT_LEFT;
		check(JoystickHatEvent(sdl), "joy2 hat1 left");
		sdl.jhat.value = SDL_HAT_CENTERED;
		check(JoystickHatEvent(sdl), "");
		sdl.jhat.value = SDL_HAT_LEFTDOWN;
		check(JoystickHatEvent(sdl), "");

		sdl.jaxis = SDL_JoyAxisEvent{};
		sdl.jaxis.type = SDL_JOYAXISMOTION;
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
		sdl.window = SDL_WindowEvent{};
		sdl.window.type = SDL_WINDOWEVENT;
		check(WindowEvent(sdl), "");
	}
	SECTION("other") {
		check(FileDropEvent("file.ext"), "");
		check(BootEvent{}, "");
	}
}

TEST_CASE("BooleanInput: match")
{
	// define various events
	auto keyDownA = KeyDownEvent::create(SDLK_a);
	auto keyDownB = KeyDownEvent::create(SDLK_b);
	auto keyUpA   = KeyUpEvent::create(SDLK_a);
	auto keyUpB   = KeyUpEvent::create(SDLK_b);

	SDL_Event sdl;
	sdl.button = SDL_MouseButtonEvent{};
	sdl.button.type = SDL_MOUSEBUTTONDOWN;
	sdl.button.button = 1;
	auto mouseDown1 = MouseButtonDownEvent(sdl);
	sdl.button.button = 2;
	auto mouseDown2 = MouseButtonDownEvent(sdl);

	sdl.button.type = SDL_MOUSEBUTTONUP;
	sdl.button.button = 1;
	auto mouseUp1 = MouseButtonUpEvent(sdl);
	sdl.button.button = 2;
	auto mouseUp2 = MouseButtonUpEvent(sdl);

	sdl.wheel = SDL_MouseWheelEvent{};
	sdl.wheel.type = SDL_MOUSEWHEEL;
	sdl.wheel.x = 2;
	sdl.wheel.y = 4;
	auto mouseWheel = MouseWheelEvent(sdl);

	sdl.jbutton = SDL_JoyButtonEvent{};
	sdl.jbutton.type = SDL_JOYBUTTONDOWN;
	sdl.jbutton.which = 0;
	sdl.jbutton.button = 2;
	auto joy1button2Down = JoystickButtonDownEvent(sdl);
	sdl.jbutton.type = SDL_JOYBUTTONUP;
	auto joy1button2Up = JoystickButtonUpEvent(sdl);

	sdl.jbutton.type = SDL_JOYBUTTONDOWN;
	sdl.jbutton.which = 0;
	sdl.jbutton.button = 3;
	auto joy1button3Down = JoystickButtonDownEvent(sdl);
	sdl.jbutton.type = SDL_JOYBUTTONUP;
	auto joy1button3Up = JoystickButtonUpEvent(sdl);

	sdl.jbutton.type = SDL_JOYBUTTONDOWN;
	sdl.jbutton.which = 1;
	sdl.jbutton.button = 2;
	auto joy2button2Down = JoystickButtonDownEvent(sdl);
	sdl.type = SDL_JOYBUTTONUP;
	auto joy2button2Up = JoystickButtonUpEvent(sdl);

	sdl.jhat = SDL_JoyHatEvent{};
	sdl.jhat.type = SDL_JOYHATMOTION;
	sdl.jhat.which = 0;
	sdl.jhat.hat = 1;
	sdl.jhat.value = SDL_HAT_LEFT;
	auto joy1hat1left = JoystickHatEvent(sdl);
	sdl.jhat.value = SDL_HAT_LEFTDOWN;
	auto joy1hat1leftDown = JoystickHatEvent(sdl);
	sdl.jhat.value = SDL_HAT_DOWN;
	auto joy1hat1down = JoystickHatEvent(sdl);

	sdl.jhat.type = SDL_JOYHATMOTION;
	sdl.jhat.which = 0;
	sdl.jhat.hat = 2;
	sdl.jhat.value = SDL_HAT_LEFT;
	auto joy1hat2left = JoystickHatEvent(sdl);
	sdl.jhat.value = SDL_HAT_LEFTDOWN;
	auto joy1hat2leftDown = JoystickHatEvent(sdl);
	sdl.jhat.value = SDL_HAT_DOWN;
	auto joy1hat2down = JoystickHatEvent(sdl);

	sdl.jhat.type = SDL_JOYHATMOTION;
	sdl.jhat.which = 1;
	sdl.jhat.hat = 1;
	sdl.jhat.value = SDL_HAT_LEFT;
	auto joy2hat1left = JoystickHatEvent(sdl);
	sdl.jhat.value = SDL_HAT_LEFTDOWN;
	auto joy2hat1leftDown = JoystickHatEvent(sdl);
	sdl.jhat.value = SDL_HAT_DOWN;
	auto joy2hat1down = JoystickHatEvent(sdl);

	sdl.jaxis = SDL_JoyAxisEvent{};
	sdl.jaxis.type = SDL_JOYAXISMOTION;
	sdl.jaxis.which = 0;
	sdl.jaxis.axis = 1;
	sdl.jaxis.value = 32000;
	auto joy1axis1P32000 = JoystickAxisMotionEvent(sdl);
	sdl.jaxis.value = 123;
	auto joy1axis1P123 = JoystickAxisMotionEvent(sdl);
	sdl.jaxis.value = -27000;
	auto joy1axis1M27000 = JoystickAxisMotionEvent(sdl);

	sdl.jaxis.type = SDL_JOYAXISMOTION;
	sdl.jaxis.which = 0;
	sdl.jaxis.axis = 2;
	sdl.jaxis.value = 32000;
	auto joy1axis2P32000 = JoystickAxisMotionEvent(sdl);
	sdl.jaxis.value = 123;
	auto joy1axis2P123 = JoystickAxisMotionEvent(sdl);
	sdl.jaxis.value = -27000;
	auto joy1axis2M27000 = JoystickAxisMotionEvent(sdl);

	sdl.jaxis.type = SDL_JOYAXISMOTION;
	sdl.jaxis.which = 1;
	sdl.jaxis.axis = 1;
	sdl.jaxis.value = 32000;
	auto joy2axis1P32000 = JoystickAxisMotionEvent(sdl);
	sdl.jaxis.value = 123;
	auto joy2axis1P123 = JoystickAxisMotionEvent(sdl);
	sdl.jaxis.value = -27000;
	auto joy2axis1M27000 = JoystickAxisMotionEvent(sdl);

	// check against various BooleanInputs
	auto getJoyDeadZone = [](JoystickId /*joystick*/) { return 25; };
	auto check = [&](const std::optional<BooleanInput>& binding, const Event& event, std::optional<bool> expected) {
		REQUIRE(binding);
		CHECK(match(*binding, event, getJoyDeadZone) == expected);
	};

	auto bKeyA = parseBooleanInput("keyb A");
	check(bKeyA, keyDownA, true);
	check(bKeyA, keyUpA, false);
	check(bKeyA, keyDownB, {});
	check(bKeyA, keyUpB, {});
	check(bKeyA, joy1button2Down, {});
	check(bKeyA, mouseWheel, {});

	auto bMouseButton1 = parseBooleanInput("mouse button1");
	check(bMouseButton1, mouseDown1, true);
	check(bMouseButton1, mouseUp1, false);
	check(bMouseButton1, mouseDown2, {});
	check(bMouseButton1, mouseUp2, {});
	check(bMouseButton1, keyDownA, {});
	check(bMouseButton1, mouseWheel, {});

	auto bJoy1Button2 = parseBooleanInput("joy1 button2");
	check(bJoy1Button2, joy1button2Down, true);
	check(bJoy1Button2, joy1button2Up, false);
	check(bJoy1Button2, joy1button3Down, {});
	check(bJoy1Button2, joy1button3Up, {});
	check(bJoy1Button2, joy2button2Down, {});
	check(bJoy1Button2, joy2button2Up, {});
	check(bJoy1Button2, mouseUp2, {});
	check(bJoy1Button2, mouseWheel, {});

	auto bJoy1Hat1Left = parseBooleanInput("joy1 hat1 left");
	check(bJoy1Hat1Left, joy1hat1left, true);
	check(bJoy1Hat1Left, joy1hat1leftDown, true);
	check(bJoy1Hat1Left, joy1hat1down, false);
	check(bJoy1Hat1Left, joy1hat2left, {});
	check(bJoy1Hat1Left, joy1hat2leftDown, {});
	check(bJoy1Hat1Left, joy1hat2down, {});
	check(bJoy1Hat1Left, joy2hat1left, {});
	check(bJoy1Hat1Left, joy2hat1leftDown, {});
	check(bJoy1Hat1Left, joy2hat1down, {});
	check(bJoy1Hat1Left, joy1button2Down, {});
	check(bJoy1Hat1Left, mouseWheel, {});

	auto bJoy1Axis1P = parseBooleanInput("joy1 +axis1");
	check(bJoy1Axis1P, joy1axis1P32000, true);
	check(bJoy1Axis1P, joy1axis1P123, false);
	check(bJoy1Axis1P, joy1axis1M27000, false);
	check(bJoy1Axis1P, joy1axis2P32000, {});
	check(bJoy1Axis1P, joy1axis2P123, {});
	check(bJoy1Axis1P, joy1axis2M27000, {});
	check(bJoy1Axis1P, joy2axis1P32000, {});
	check(bJoy1Axis1P, joy2axis1P123, {});
	check(bJoy1Axis1P, joy2axis1M27000, {});
	check(bJoy1Axis1P, joy1hat1left, {});
	check(bJoy1Axis1P, mouseWheel, {});
}
