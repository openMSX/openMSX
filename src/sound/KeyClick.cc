// $Id$

#include "KeyClick.hh"


namespace openmsx {

KeyClick::KeyClick(const XMLElement& config, const EmuTime& time)
	: dac("keyclick", "1-bit click generator", config, time)
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
		dac.writeDAC((status ? 0xff : 0x80), time);
	}
}

} // namespace openmsx
