// $Id$

#include "MSXYM2413.hh"
#include "MSXMotherBoard.hh"
#include "Mixer.hh"
#include "YM2413.hh"

MSXYM2413::MSXYM2413(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time)
{
	PRT_DEBUG("Creating an MSXYM2413 object");
	
	MSXMotherBoard::instance()->register_IO_Out(0x7c, this);
	MSXMotherBoard::instance()->register_IO_Out(0x7d, this);
	short volume = (short)deviceConfig->getParameterAsInt("volume");
	ym2413 = new YM2413(volume, time);
	reset(time);
}

MSXYM2413::~MSXYM2413()
{
	PRT_DEBUG("Destroying an MSXYM2413 object");
	delete ym2413;
}

void MSXYM2413::reset(const EmuTime &time)
{
	ym2413->reset(time);
	registerLatch = 0; // TODO check
	enable = 0; 	// TODO check
}

void MSXYM2413::writeIO(byte port, byte value, const EmuTime &time)
{
	if (enable&0x01) {
		switch(port) {
		case 0x7c:
			writeRegisterPort(value, time);
			break;
		case 0x7d:
			writeDataPort(value, time);
			break;
		default:
			assert(false);
		}
	}
}

void MSXYM2413::writeRegisterPort(byte value, const EmuTime &time)
{
	registerLatch = (value & 0x3f);
}
void MSXYM2413::writeDataPort(byte value, const EmuTime &time)
{
	Mixer::instance()->updateStream(time);
	ym2413->writeReg(registerLatch, value, time);
}
