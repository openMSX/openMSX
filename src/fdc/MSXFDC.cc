// $Id$

#include "MSXFDC.hh"
#include "Rom.hh"
#include "RealDrive.hh"
#include "XMLElement.hh"
#include "StringOp.hh"
#include "MSXException.hh"

namespace openmsx {

MSXFDC::MSXFDC(MSXMotherBoard& motherBoard, const XMLElement& config)
	: MSXDevice(motherBoard, config)
	, rom(new Rom(motherBoard, getName() + " ROM", "rom", config))
{
	bool singleSided = config.findChild("singlesided");
	int numDrives = config.getChildDataAsInt("drives", 1);
	if ((0 >= numDrives) || (numDrives >= 4)) {
		throw MSXException("Invalid number of drives: " +
		                   StringOp::toString(numDrives));
	}
	int i = 0;
	for ( ; i < numDrives; ++i) {
		if (singleSided) {
			drives[i].reset(new SingleSidedDrive(getMotherBoard()));
		} else {
			drives[i].reset(new DoubleSidedDrive(getMotherBoard()));
		}
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
	return *MSXFDC::getReadCacheLine(address);
}

byte MSXFDC::peekMem(word address, const EmuTime& /*time*/) const
{
	return *MSXFDC::getReadCacheLine(address);
}

const byte* MSXFDC::getReadCacheLine(word start) const
{
	return &(*rom)[start & 0x3FFF];
}

} // namespace openmsx
