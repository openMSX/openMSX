// $Id$

#include "MSXFDC.hh"
#include "DiskDrive.hh"
#include "xmlx.hh"

namespace openmsx {

MSXFDC::MSXFDC(const XMLElement& config, const EmuTime& time)
	: MSXDevice(config, time), MSXMemDevice(config, time),
	  rom(getName() + " ROM", "rom", config) 
{
	string drivename("drivename1");
	//                0123456789
	for (int i = 0; i < 4; i++) {
		drivename[9] = '1' + i;
		const XMLElement* driveElem = config.findChild(drivename);
		if (driveElem) {
			drives[i] = new DoubleSidedDrive(
			                     driveElem->getData(), time);
		} else {
			drives[i] = new DummyDrive();
		}
	}
}

MSXFDC::~MSXFDC()
{
	for (int i = 0; i < 4; i++) {
		delete drives[i];
	}
}

byte MSXFDC::readMem(word address, const EmuTime& /*time*/)
{
	return rom[address & 0x3FFF];
}

const byte* MSXFDC::getReadCacheLine(word start) const
{
	return &rom[start & 0x3FFF];
}

} // namespace openmsx
