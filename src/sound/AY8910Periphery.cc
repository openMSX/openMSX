#include "AY8910Periphery.hh"

namespace openmsx {

uint8_t AY8910Periphery::readA(EmuTime::param /*time*/)
{
	return 0xFF; // unused bits are 1
}

uint8_t AY8910Periphery::readB(EmuTime::param /*time*/)
{
	return 0xFF; // unused bits are 1
}

void AY8910Periphery::writeA(uint8_t /*value*/, EmuTime::param /*time*/)
{
	// nothing connected
}

void AY8910Periphery::writeB(uint8_t /*value*/, EmuTime::param /*time*/)
{
	// nothing connected
}

} // namespace openmsx
