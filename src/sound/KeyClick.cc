// $Id$

#include "KeyClick.hh"
#include "DACSound8U.hh"

namespace openmsx {

KeyClick::KeyClick(MSXMixer& mixer, const XMLElement& config)
	: dac(new DACSound8U(mixer, "keyclick", "1-bit click generator", config))
	, status(false)
{
}

KeyClick::~KeyClick()
{
}

void KeyClick::reset(const EmuTime& time)
{
	setClick(false, time);
}

void KeyClick::setClick(bool newStatus, const EmuTime& time)
{
	if (newStatus != status) {
		status = newStatus;
		dac->writeDAC((status ? 0xff : 0x80), time);
	}
}

// We don't need a serialize() method, instead the setClick() method
// gets called during de-serialization.

} // namespace openmsx
