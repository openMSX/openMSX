// $Id$

#include "Y8950Periphery.hh"
#include "MSXDevice.hh"

namespace openmsx {

Y8950Periphery::~Y8950Periphery()
{
}

void Y8950Periphery::reset()
{
	// nothing
}

void Y8950Periphery::setSPOFF(bool /*value*/, const EmuTime& /*time*/)
{
	// nothing
}

byte Y8950Periphery::readMem(word /*address*/, const EmuTime& /*time*/)
{
	return 0xFF;
}
void Y8950Periphery::writeMem(word /*address*/, byte /*value*/, const EmuTime& /*time*/)
{
	// nothing
}
const byte* Y8950Periphery::getReadCacheLine(word /*start*/) const
{
	return MSXDevice::unmappedRead;
}
byte* Y8950Periphery::getWriteCacheLine(word /*start*/) const
{
	return MSXDevice::unmappedWrite;
}

} // namespace openmsx
