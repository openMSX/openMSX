#include "SG1000JoystickIO.hh"

#include "JoystickPort.hh"

#include "MSXMotherBoard.hh"
#include "serialize.hh"

#include "narrow.hh"

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

template<typename ReadJoy>
static byte readJoysticks(uint16_t port, ReadJoy readJoy)
{
	if (port & 1) {
		byte joy2 = readJoy(1) & 0x3F;
		return 0xF0 | (joy2 >> 2);
	} else {
		byte joy1 = readJoy(0) & 0x3F;
		byte joy2 = readJoy(1) & 0x3F;
		return narrow_cast<byte>(joy1 | (joy2 << 6));
	}
}

byte SG1000JoystickIO::readIO(uint16_t port, EmuTime time)
{
	return readJoysticks(port, [&](int i) { return ports[i]->read(time); });
}

byte SG1000JoystickIO::peekIO(uint16_t port, EmuTime time) const
{
	return readJoysticks(port, [&](int i) { return ports[i]->peek(time); });
}


template<typename Archive>
void SG1000JoystickIO::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
}
INSTANTIATE_SERIALIZE_METHODS(SG1000JoystickIO);
REGISTER_MSXDEVICE(SG1000JoystickIO, "SG1000Joystick");

} // namespace openmsx
