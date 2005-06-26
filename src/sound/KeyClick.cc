// $Id$

#include "KeyClick.hh"
#include "DACSound8U.hh"


namespace openmsx {

KeyClick::KeyClick(Mixer& mixer, const XMLElement& config, const EmuTime& time)
	: dac(new DACSound8U(mixer, "keyclick", "1-bit click generator",
	                     config, time))
{
	status = false;
}

KeyClick::~KeyClick()
{
}

void KeyClick::reset(const EmuTime &time)
{
	setClick(false, time);
}

void KeyClick::setClick(bool newStatus, const EmuTime &time)
{
	if (newStatus != status) {
		status = newStatus;
		dac->writeDAC((status ? 0xff : 0x80), time);
	}
}

} // namespace openmsx
