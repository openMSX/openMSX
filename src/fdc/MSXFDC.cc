// $Id$

#include "MSXFDC.hh"
#include "Rom.hh"
#include "DiskDrive.hh"
#include "XMLElement.hh"
#include "StringOp.hh"
#include "MSXException.hh"
#include "MSXMotherBoard.hh"

namespace openmsx {

MSXFDC::MSXFDC(MSXMotherBoard& motherBoard, const XMLElement& config,
               const EmuTime& time)
	: MSXDevice(motherBoard, config, time)
	, rom(new Rom(motherBoard, getName() + " ROM", "rom", config))
{
	int numDrives = config.getChildDataAsInt("drives", 1);
	if ((0 >= numDrives) || (numDrives >= 4)) {
		throw MSXException("Invalid number of drives: " +
		                   StringOp::toString(numDrives));
	}
	int i = 0;
	for ( ; i < numDrives; ++i) {
		drives[i].reset(new DoubleSidedDrive(
			getMotherBoard().getCommandController(),
			getMotherBoard().getEventDistributor(),
			getMotherBoard().getScheduler(),
			getMotherBoard().getFileManipulator(),
			time));
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
