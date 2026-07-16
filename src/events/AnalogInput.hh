#ifndef ANALOG_INPUT_HH
#define ANALOG_INPUT_HH

// A 'AnalogInput' represents an input with an associated analog value
//
// Some examples:
// * Joystick axis motion, the corresponding analog value can vary between -32768..32767
// * Relative mouse motion, separate for X and Y axis.

#include "Event.hh"
#include "JoystickId.hh"

#include "function_ref.hh"

#include <SDL.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace openmsx {

class AnalogMouseAxis
{
public:
	// 0 = X-axis, 1 = Y-axis
	explicit AnalogMouseAxis(uint8_t axis_)
		: axis(axis_) {}

	[[nodiscard]] auto getAxis() const { return axis; }

private:
	uint8_t axis;
};

class AnalogJoystickAxis
{
public:
	explicit AnalogJoystickAxis(JoystickId joystick_, uint8_t axis_)
		: joystick(joystick_), axis(axis_) {}

	[[nodiscard]] auto getJoystick() const { return joystick; }
	[[nodiscard]] auto getAxis() const { return axis; }

private:
	JoystickId joystick;
	uint8_t axis;
};


using AnalogInput = std::variant<
	AnalogMouseAxis,
	AnalogJoystickAxis>;


[[nodiscard]] std::string toString(const AnalogInput& input);
[[nodiscard]] std::optional<AnalogInput> parseAnalogInput(std::string_view text);
[[nodiscard]] std::optional<AnalogInput> captureAnalogInput(const Event& event, function_ref<int(JoystickId)> getJoyDeadZone);

[[nodiscard]] bool operator==(const AnalogInput& x, const AnalogInput& y);

[[nodiscard]] std::optional<int> match(const AnalogInput& binding, const Event& event,
                                       function_ref<int(JoystickId)> getJoyDeadZone);

} // namespace openmsx

#endif
