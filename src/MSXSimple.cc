#include <cassert>
#include "MSXSimple.hh"
#include "MSXMotherBoard.hh"
#include "DACSound.hh"

MSXSimple::MSXSimple(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time)
{
	PRT_DEBUG("Creating an MSXSimple");

	//only data bits of the printport are used
	MSXMotherBoard::instance()->register_IO_Out(0x91, this);
	MSXMotherBoard::instance()->register_IO_In (0x91, this);

	short volume = (short)config->getParameterAsInt("volume");
	dac = new DACSound(volume, 16000, time);

	reset(time);
}

MSXSimple::~MSXSimple()
{
	delete dac;
}

void MSXSimple::reset(const EmuTime &time)
{
  dac->reset(time);
}


byte MSXSimple::readIO(byte port, const EmuTime &time)
{
	return 0;
}

void MSXSimple::writeIO(byte port, byte value, const EmuTime &time)
{
	dac->writeDAC(value,time);
}
