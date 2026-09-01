#ifndef JOYSTICK_ID_HH
#define JOYSTICK_ID_HH

#include "StringOp.hh"
#include "strCat.hh"

#include <optional>
#include <string_view>

namespace openmsx {

// openMSX specific joystick numbering,
//  different from SDL joystick device index and
//  different from SDL joystick instance ID.
class JoystickId {
public:
	static constexpr std::string_view PREFIX = "joy";

	explicit JoystickId(unsigned id_) : id(id_) {}
	[[nodiscard]] unsigned raw() const { return id; }
	[[nodiscard]] bool operator==(const JoystickId&) const = default;
	[[nodiscard]] std::string str() const { return strCat(PREFIX, id + 1); }

	[[nodiscard]] static std::optional<JoystickId> parse(std::string_view str) {
		if (!str.starts_with(PREFIX)) return std::nullopt;
		str.remove_prefix(PREFIX.size());
		auto n = StringOp::stringToBase<10, unsigned>(str);
		if (!n || *n == 0) return std::nullopt;
		return JoystickId(*n - 1);
	}

private:
	unsigned id;
};

} // namespace openmsx

#endif
