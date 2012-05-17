// $Id$

#include "KeyClick.hh"
#include "DACSound8U.hh"

namespace openmsx {

KeyClick::KeyClick(const DeviceConfig& config)
	: dac(new DACSound8U("keyclick", "1-bit click generator", config))
	, status(false)
{
}

KeyClick::~KeyClick()
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
		dac->writeDAC((status ? 0xff : 0x80), time);
	}
}

// We don't need a serialize() method, instead the setClick() method
// gets called during de-serialization.

} // namespace openmsx
