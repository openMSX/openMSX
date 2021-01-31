#include "SETetrisDongle.hh"
#include "serialize.hh"
#include "serialize_meta.hh"

namespace openmsx {

SETetrisDongle::SETetrisDongle()
{
	status = JOY_UP | JOY_DOWN | JOY_LEFT | JOY_RIGHT |
	         JOY_BUTTONA | JOY_BUTTONB;
}

// Pluggable
std::string_view SETetrisDongle::getName() const
{
	return "tetris2-protection";
}

std::string_view SETetrisDongle::getDescription() const
{
	return "Tetris II Special Edition dongle";
}

void SETetrisDongle::plugHelper(
	Connector& /*connector*/, EmuTime::param /*time*/)
{
}

void SETetrisDongle::unplugHelper(EmuTime::param /*time*/)
{
}


// JoystickDevice
byte SETetrisDongle::read(EmuTime::param /*time*/)
{
	return status;
}

void SETetrisDongle::write(byte value, EmuTime::param /*time*/)
{
	// Original device used 4 NOR ports
	// pin4 will be value of pin7
	if (value & 2) {
		status |= JOY_RIGHT;
	} else {
		status &= ~JOY_RIGHT;
	}
}

template<typename Archive>
void SETetrisDongle::serialize(Archive& /*ar*/, unsigned /*version*/)
{
	// no need to serialize 'status', port will anyway be re-written
	// on de-serialize
}
INSTANTIATE_SERIALIZE_METHODS(SETetrisDongle);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, SETetrisDongle, "SETetrisDongle");

} // namespace openmsx
