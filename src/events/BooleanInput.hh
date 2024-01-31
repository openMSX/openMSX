#ifndef BOOLEAN_INPUT_HH
#define BOOLEAN_INPUT_HH

// A 'BooleanInput' represents an input which has a well defined start and stop.
//
// Some examples:
// * Press a key on the keyboard, later release that key.
// * Same for joystick and mouse buttons.
// * The 4 directions on a joystick hat (d-pad): active when e.g. pressing the
//   'up' direction, it remains active when the 'up-left' or 'up-right'
//   combination is pressed, but becomes deactive for any other combination
//   (including 'center').
// * Joystick axis: becomes active when moving the axis in e.g. the positive
//   direction (the negative direction is a different BooleanInput) past the
//   dead-zone in the center. Becomes deactive when moving the axis back to the
//   center or in the opposite direction.
//
// Counter-examples:
// * Mouse-wheel.
// * Mouse motion.

#include "Event.hh"
#include "JoystickId.hh"

#include <SDL.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace openmsx {

class BooleanKeyboard
{
public:
	explicit BooleanKeyboard(SDL_Keycode code)
		: keyCode(code) {}

	[[nodiscard]] auto getKeyCode() const { return keyCode; }

private:
	SDL_Keycode keyCode;
};

class BooleanMouseButton
{
public:
	explicit BooleanMouseButton(uint8_t button_)
		: button(button_) {}

	[[nodiscard]] auto getButton() const { return button; }

private:
	uint8_t button;
};

class BooleanJoystickButton
{
public:
	explicit BooleanJoystickButton(JoystickId joystick_, uint8_t button_)
		: joystick(joystick_), button(button_) {}

	[[nodiscard]] auto getJoystick() const { return joystick; }
	[[nodiscard]] auto getButton() const { return button; }

private:
	JoystickId joystick;
	uint8_t button;
};

class BooleanJoystickHat
{
public:
	enum Value : uint8_t {
		UP    = SDL_HAT_UP,
		RIGHT = SDL_HAT_RIGHT,
		DOWN  = SDL_HAT_DOWN,
		LEFT  = SDL_HAT_LEFT,
	};

	explicit BooleanJoystickHat(JoystickId joystick_, uint8_t hat_, Value value_)
		: joystick(joystick_), hat(hat_), value(value_) {}

	[[nodiscard]] auto getJoystick() const { return joystick; }
	[[nodiscard]] auto getHat() const { return hat; }
	[[nodiscard]] auto getValue() const { return value; }

private:
	JoystickId joystick;
	uint8_t hat;
	Value value;
};

class BooleanJoystickAxis
{
public:
	enum Direction : uint8_t { POS = 0, NEG = 1 };

	explicit BooleanJoystickAxis(JoystickId joystick_, uint8_t axis_, Direction direction_)
		: joystick(joystick_), axis(axis_), direction(direction_) {}

	[[nodiscard]] auto getJoystick() const { return joystick; }
	[[nodiscard]] auto getAxis() const { return axis; }
	[[nodiscard]] auto getDirection() const { return direction; }

private:
	JoystickId joystick;
	uint8_t axis;
	Direction direction;
};


using BooleanInput = std::variant<
	BooleanKeyboard,
	BooleanMouseButton,
	BooleanJoystickButton,
	BooleanJoystickHat,
	BooleanJoystickAxis>;


[[nodiscard]] std::string toString(const BooleanInput& input);
[[nodiscard]] std::optional<BooleanInput> parseBooleanInput(std::string_view text);
[[nodiscard]] std::optional<BooleanInput> captureBooleanInput(const Event& event, std::function<int(JoystickId)> getJoyDeadZone);

[[nodiscard]] bool operator==(const BooleanInput& x, const BooleanInput& y);

[[nodiscard]] std::optional<bool> match(const BooleanInput& binding, const Event& event,
                                        std::function<int(JoystickId)> getJoyDeadZone);

} // namespace openmsx

#endif
