// $Id$

#include "MSXFDC.hh"
#include "DiskDrive.hh"
#include "DiskImageCLI.hh"

DiskImageCLI diskImageCLI;


MSXFDC::MSXFDC(Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXMemDevice(config, time),
	  rom(config, time) 
{
	std::string drivename("drivename1");
	//                     0123456789
	for (int i = 0; i < 4; i++) {
		drivename[9] = '1' + i;
		if (config->hasParameter(drivename)) {
			drives[i] = new DoubleSidedDrive(
			                config->getParameter(drivename), time);
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

byte MSXFDC::readMem(word address, const EmuTime &time) const
{
	return rom.read(address & 0x3FFF);
}

const byte* MSXFDC::getReadCacheLine(word start) const
{
	return rom.getBlock(start & 0x3FFF);
}
