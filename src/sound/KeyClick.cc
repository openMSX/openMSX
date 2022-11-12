#include "KeyClick.hh"

namespace openmsx {

KeyClick::KeyClick(const DeviceConfig& config)
	: dac("keyclick", "1-bit click generator", config)
{
}

void KeyClick::reset(EmuTime::param time)
{
	setClick(false, time);
}

void KeyClick::setClick(bool newStatus, EmuTime::param time)
{
	if (newStatus != status) {
		status = newStatus;
		dac.writeDAC((status ? 0xff : 0x80), time);
	}
}

// We don't need a serialize() method, instead the setClick() method
// gets called during de-serialization.

} // namespace openmsx
