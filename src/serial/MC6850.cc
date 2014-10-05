#include "MC6850.hh"
#include "EmuTime.hh"
#include "serialize.hh"
#include "unreachable.hh"

namespace openmsx {

// Dummy implementation, just enough to make some Music-Module detection
// routines pass:
// - fac demo 5: does OUT 0,3 : OUT 0,21 : INP(4) and expects to read 2
// - tetris 2 special edition: does INP(4) and expects to read 0
// I did some _very_basic_ investigation and found the following:
// - after a reset reading from port 4 returns 0
// - after initializing the control register, reading port 4 returns 0 (of
//   course this will change when you start to actually receive/transmit data)
// - writing any value with the lower 2 bits set to 1 returns to the initial
//   state, and reading port 4 again returns 0.

MC6850::MC6850(const DeviceConfig& config)
	: MSXDevice(config)
{
	reset(EmuTime::dummy());
}

void MC6850::reset(EmuTime::param /*time*/)
{
	control = 3;
}

byte MC6850::readIO(word port, EmuTime::param time)
{
	return peekIO(port, time);
}

byte MC6850::peekIO(word port, EmuTime::param /*time*/) const
{
	byte result;
	switch (port & 0x1) {
	case 0: // read status
		result = ((control & 3) == 3) ? 0 : 2;
		break;
	case 1: // read data
		result = 0;
		break;
	default: // unreachable, avoid warning
		UNREACHABLE; result = 0;
	}
	return result;
}

void MC6850::writeIO(word port, byte value, EmuTime::param /*time*/)
{
	switch (port & 0x01) {
	case 0: // control
		control = value;
		break;
	case 1: // write data
		break;
	}
}

// version 1: initial version
// version 2: added control
template<typename Archive>
void MC6850::serialize(Archive& ar, unsigned version)
{
	ar.template serializeBase<MSXDevice>(*this);
	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("control", control);
	} else {
		control = 3;
	}
}
INSTANTIATE_SERIALIZE_METHODS(MC6850);
REGISTER_MSXDEVICE(MC6850, "MC6850");

} // namespace openmsx
