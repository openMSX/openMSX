// $Id$

#include "MSXFDC.hh"
#include "Rom.hh"
#include "DiskDrive.hh"
#include "XMLElement.hh"
#include "StringOp.hh"
#include "MSXException.hh"

namespace openmsx {

MSXFDC::MSXFDC(const XMLElement& config, const EmuTime& time)
	: MSXDevice(config, time)
	, rom(new Rom(getName() + " ROM", "rom", config))
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

void MSXFDC::powerDown(const EmuTime& time)
{
	for (int i = 0; i < 4; ++i) {
		drives[i]->setMotor(false, time);
	}
}

byte MSXFDC::readMem(word address, const EmuTime& /*time*/)
{
	return *getReadCacheLine(address);
}

const byte* MSXFDC::getReadCacheLine(word start) const
{
	return &(*rom)[start & 0x3FFF];
}

} // namespace openmsx
