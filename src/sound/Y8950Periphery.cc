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

void Y8950Periphery::setSPOFF(bool /*value*/, EmuTime::param /*time*/)
{
	// nothing
}

byte Y8950Periphery::readMem(word address, EmuTime::param time)
{
	// by default do same as peekMem()
	return peekMem(address, time);
}
byte Y8950Periphery::peekMem(word /*address*/, EmuTime::param /*time*/) const
{
	return 0xFF;
}
void Y8950Periphery::writeMem(word /*address*/, byte /*value*/, EmuTime::param /*time*/)
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
