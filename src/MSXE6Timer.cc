// $Id$

#include <cassert>
#include "MSXE6Timer.hh"

namespace openmsx {

MSXE6Timer::MSXE6Timer(Config *config, const EmuTime& time)
	: MSXDevice(config, time), MSXIODevice(config, time)
{
	reset(time);
}

MSXE6Timer::~MSXE6Timer()
{
}

void MSXE6Timer::reset(const EmuTime& time)
{
	reference = time;
}

void MSXE6Timer::writeIO(byte port, byte value, const EmuTime& time)
{
	reference = time;
}

byte MSXE6Timer::readIO(byte port, const EmuTime& time)
{
	int counter = reference.getTicksTill(time);
	switch (port & 0x01) {
	case 0:
		return counter & 0xFF;
	case 1:
		return (counter >> 8) & 0xFF;
	default: // unreachable, avoid warning
		assert(false);
		return 0;
	}
}

} // namespace openmsx

