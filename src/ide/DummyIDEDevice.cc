// $Id$

#include "DummyIDEDevice.hh"


namespace openmsx {

void DummyIDEDevice::reset(const EmuTime &time)
{
	// do nothing
}

word DummyIDEDevice::readData(const EmuTime &time)
{
	return 0x7F7F;
}

byte DummyIDEDevice::readReg(nibble reg, const EmuTime &time)
{
	return 0x7F;
}

void DummyIDEDevice::writeData(word value, const EmuTime &time)
{
	// do nothing
}

void DummyIDEDevice::writeReg(nibble reg, byte value, const EmuTime &time)
{
	// do nothing
}

} // namespace openmsx
