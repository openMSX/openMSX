#include "AnalogInput.hh"

#include "StringOp.hh"
#include "one_of.hh"
#include "stl.hh"

#include <cctype>
#include <tuple>

namespace openmsx {

std::string toString(const AnalogInput& input)
{
	return std::visit(overloaded{
		[](const AnalogMouseAxis& m) {
			return strCat("mouse ", (m.getAxis() ? 'Y' : 'X'), "-axis");
		},
		[](const AnalogJoystickAxis& j) {
			return strCat(j.getJoystick().str(), " axis", j.getAxis());
		},
	}, input);
}

std::optional<AnalogInput> parseAnalogInput(std::string_view text)
{
	auto tokenizer = StringOp::split_view<StringOp::EmptyParts::REMOVE>(text, ' ');
	auto it = tokenizer.begin();
	auto et = tokenizer.end();

	if (it == et) return std::nullopt;
	auto type = *it++;
	if (it == et) return std::nullopt;
	if (type == "mouse") {
		auto axis = *it++;
		if (it != et) return std::nullopt;
		if (axis.size() != 6 || !axis.ends_with("-axis")) return std::nullopt;
		auto n = tolower(axis[0]) - 'x';
		if (n != one_of(0, 1)) return std::nullopt;
		return AnalogMouseAxis(n);
	} else if (type.starts_with("joy")) {
		type.remove_prefix(3);
		auto j = StringOp::stringToBase<10, unsigned>(type);
		if (!j || *j == 0) return std::nullopt;
		auto joyId = JoystickId(*j - 1);

		auto axis = *it++;
		if (it != et) return std::nullopt;
		if (!axis.starts_with("axis")) return std::nullopt;
		axis.remove_prefix(4);
		auto a = StringOp::stringToBase<10, unsigned>(axis);
		if (!a) return std::nullopt;
		return AnalogJoystickAxis(joyId, *a);
	}
	return std::nullopt;

}

static constexpr int mouseThreshold = 5;
std::optional<AnalogInput> captureAnalogInput(const Event& event, function_ref<int(JoystickId)> getJoyDeadZone)
{
	return std::visit(overloaded{
		[](const MouseMotionEvent& e) -> std::optional<AnalogInput> {
			int x = std::abs(e.getX());
			int y = std::abs(e.getY());
			if (x > y && x >= mouseThreshold) return AnalogMouseAxis(0);
			if (y > x && y >= mouseThreshold) return AnalogMouseAxis(1);
			return std::nullopt;
		},
		[&](const JoystickAxisMotionEvent& e) -> std::optional<AnalogInput> {
			auto joyId = e.getJoystick();
			int deadZone = getJoyDeadZone(joyId); // percentage 0..100
			int threshold = (deadZone * 32768) / 100;
			auto value = std::abs(e.getValue());
			if (value <= threshold) return std::nullopt;
			return AnalogJoystickAxis(joyId, e.getAxis());
		},
		[](const EventBase&) -> std::optional<AnalogInput> {
			return std::nullopt;
		}
	}, event);
}

bool operator==(const AnalogInput& x, const AnalogInput& y)
{
	return std::visit(overloaded{
		[](const AnalogMouseAxis& m1, const AnalogMouseAxis& m2) {
			return m1.getAxis() == m2.getAxis();
		},
		[](const AnalogJoystickAxis& j1, const AnalogJoystickAxis& j2) {
			return std::tuple(j1.getJoystick(), j1.getAxis()) ==
			       std::tuple(j2.getJoystick(), j2.getAxis());
		},
		[](const auto&, const auto&) { // mixed types
			return false;
		}
	}, x, y);
}


std::optional<int> match(const AnalogInput& binding, const Event& event,
                          function_ref<int(JoystickId)> getJoyDeadZone)
{
	return std::visit(overloaded{
		[](const AnalogMouseAxis& bind, const MouseMotionEvent& mouse) -> std::optional<int> {
			auto a = bind.getAxis();
			if (a != one_of(0, 1)) return std::nullopt;
			auto d = a ? mouse.getY() : mouse.getX();
			return (std::abs(d) < mouseThreshold) ? 0 : d;
		},
		[&](const AnalogJoystickAxis& bind, const JoystickAxisMotionEvent& e) -> std::optional<int> {
			if (bind.getJoystick() != e.getJoystick()) return std::nullopt;
			if (bind.getAxis() != e.getAxis()) return std::nullopt;
			int deadZone = getJoyDeadZone(bind.getJoystick()); // percentage 0..100
			int threshold = (deadZone * 32768) / 100;
			int v = e.getValue();
			return (std::abs(v) < threshold) ? 0 : v;
		},
		[](const auto& /*bind*/, const auto& /*event*/) -> std::optional<int> {
			return std::nullopt;
		}
	}, binding, event);
}

} // namespace openmsx
