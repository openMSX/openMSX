// $Id$

#include "MSXFDC.hh"
#include "DiskDrive.hh"
#include "xmlx.hh"
#include "StringOp.hh"

namespace openmsx {

MSXFDC::MSXFDC(const XMLElement& config, const EmuTime& time)
	: MSXDevice(config, time)
	, rom(getName() + " ROM", "rom", config) 
{
	int numDrives = config.getChildDataAsInt("drives", 1);
	if ((0 >= numDrives) || (numDrives >= 4)) {
		throw FatalError("Invalid number of drives: " +
		                 StringOp::toString(numDrives));
	}
	int i = 0;
	for ( ; i < numDrives; ++i) {
		drives[i].reset(new DoubleSidedDrive(time));
	}
	for ( ; i < 4; ++i) {
		drives[i].reset(new DummyDrive());
	}
}

MSXFDC::~MSXFDC()
{
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
