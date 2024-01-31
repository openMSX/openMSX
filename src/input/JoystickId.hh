#ifndef JOYSTICK_ID_HH
#define JOYSTICK_ID_HH

#include "strCat.hh"

namespace openmsx {

// openMSX specific joystick numbering,
//  different from SDL joystick device index and
//  different from SDL joystick instance ID.
class JoystickId {
public:
	JoystickId(unsigned id_) : id(id_) {}
	[[nodiscard]] unsigned raw() const { return id; }
	[[nodiscard]] bool operator==(const JoystickId&) const = default;
	[[nodiscard]] std::string str() const { return strCat("joy", id + 1); }

private:
	unsigned id;
};

} // namespace openmsx

#endif
