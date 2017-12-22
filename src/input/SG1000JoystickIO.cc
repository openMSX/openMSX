#include "SG1000JoystickIO.hh"
#include "MSXMotherBoard.hh"
#include "JoystickPort.hh"
#include "serialize.hh"

namespace openmsx {

// MSXDevice
SG1000JoystickIO::SG1000JoystickIO(const DeviceConfig& config)
	: MSXDevice(config)

{
	MSXMotherBoard& motherBoard = getMotherBoard();
	auto time = getCurrentTime();
	for (unsigned i = 0; i < 2; i++) {
		ports[i] = &motherBoard.getJoystickPort(i);
		// Clear strobe bit, or MSX joysticks will stay silent.
		ports[i]->write(0xFB, time);
	}
}

SG1000JoystickIO::~SG1000JoystickIO()
{
}

byte SG1000JoystickIO::readIO(word port, EmuTime::param time)
{
	return peekIO(port, time);
}

byte SG1000JoystickIO::peekIO(word port, EmuTime::param time) const
{
	// Joystick hardware cannot detect when it's being read, so using the
	// read() method for peeking should be safe.
	if (port & 1) {
		byte joy2 = ports[1]->read(time) & 0x3F;
		return 0xF0 | (joy2 >> 2);
	} else {
		byte joy1 = ports[0]->read(time) & 0x3F;
		byte joy2 = ports[1]->read(time) & 0x3F;
		return joy1 | (joy2 << 6);
	}
}


template<typename Archive>
void SG1000JoystickIO::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
}
INSTANTIATE_SERIALIZE_METHODS(SG1000JoystickIO);
REGISTER_MSXDEVICE(SG1000JoystickIO, "SG1000Joystick");

} // namespace openmsx
