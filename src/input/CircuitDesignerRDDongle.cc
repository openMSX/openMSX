#include "CircuitDesignerRDDongle.hh"

#include "serialize.hh"
#include "serialize_meta.hh"

/*
 The circuit designer RD dongle is a simple dongle that is used to protect the software.
 It needs to be plugged in Joystick port B, when it's not plugged in the software will
 add a reset command to the BLOAD hook which will reset the MSX.

     Pin 1 = Up
     Pin 6 = Button A
     Pin 7 = Button B

     Output  Input
     pin|pin|Pin (Up)
     7 | 6 | 1
     -----------------
     0 | 0 | 0
     0 | 1 | 1
     1 | 0 | 0
     1 | 1 | 0
*/

namespace openmsx {

// Pluggable
std::string_view CircuitDesignerRDDongle::getName() const
{
	return "circuit-designer-rd-dongle";
}

std::string_view CircuitDesignerRDDongle::getDescription() const
{
	return "Circuit Designer RD dongle";
}

void CircuitDesignerRDDongle::plugHelper(
	Connector& /*connector*/, EmuTime::param /*time*/)
{
}

void CircuitDesignerRDDongle::unplugHelper(EmuTime::param /*time*/)
{
}


// JoystickDevice
uint8_t CircuitDesignerRDDongle::read(EmuTime::param /*time*/)
{
	return status;
}

void CircuitDesignerRDDongle::write(uint8_t value, EmuTime::param /*time*/)
{
	if (!(value & WR_PIN7) && (value & WR_PIN6)) {
		status |=  JOY_UP;
	} else {
		status &= ~JOY_UP;
	}
}

template<typename Archive>
void CircuitDesignerRDDongle::serialize(Archive& /*ar*/, unsigned /*version*/)
{
	// no need to serialize 'status', port will anyway be re-written
	// on de-serialize
}
INSTANTIATE_SERIALIZE_METHODS(CircuitDesignerRDDongle);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, CircuitDesignerRDDongle, "CircuitDesignerRDDongle");

} // namespace openmsx
