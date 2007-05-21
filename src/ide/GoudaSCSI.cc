// $Id$

#include "GoudaSCSI.hh"
#include "WD33C93.hh"
#include "Rom.hh"

namespace openmsx {

GoudaSCSI::GoudaSCSI(MSXMotherBoard& motherBoard, const XMLElement& config,
                     const EmuTime& time)
	: MSXDevice(motherBoard, config, time)
	, rom(new Rom(motherBoard, getName() + " ROM", "rom", config))
	, wd33c93(new WD33C93(config))
{
	reset(time);
}

GoudaSCSI::~GoudaSCSI()
{
}

void GoudaSCSI::reset(const EmuTime& time)
{
	wd33c93->reset(true);
}

byte GoudaSCSI::readIO(word port, const EmuTime& time)
{
	byte result;
	switch (port & 0x03) {
	case 0:
		result = wd33c93->readAuxStatus();
		break;
	case 1:
		result = wd33c93->readCtrl();
		break;
	case 2:
		result = 0xb0; // bit 4: 1 = Halt on SCSI parity error
		break;
	default:
		assert(false);
		result = 0;
	}
	return result;
}

void GoudaSCSI::writeIO(word port, byte value, const EmuTime& time)
{
	switch (port & 0x03) {
	case 0:
		wd33c93->writeAdr(value);
		break;
	case 1:
		wd33c93->writeCtrl(value);
		break;
	case 2:
		reset(time);
		break;
	default:
		assert(false);
	}
}

byte GoudaSCSI::readMem(word address, const EmuTime& time)
{
	return (*rom)[address & (rom->getSize() - 1)];
}

} // namespace openmsx
