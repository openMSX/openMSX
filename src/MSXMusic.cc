// $Id: MSXMusic.cc,v

#include "MSXMusic.hh"
#include "MSXMotherBoard.hh"
#include "Mixer.hh"

MSXMusic::MSXMusic()
{
	PRT_DEBUG("Creating an MSXMusic object");
}

MSXMusic::~MSXMusic()
{
	PRT_DEBUG("Destroying an MSXMusic object");
}

void MSXMusic::init()
{
	MSXDevice::init();
	MSXMotherBoard::instance()->register_IO_Out(0x7c, this);
	MSXMotherBoard::instance()->register_IO_Out(0x7d, this);
	ym2413 = new YM2413();
}

void MSXMusic::reset()
{
	ym2413->reset();
	registerLatch = 0; // TODO check
}

void MSXMusic::writeIO(byte port, byte value, Emutime &time)
{
	switch(port) {
	case 0x7c:
		registerLatch = (value & 0x3f);
		break;
	case 0x7d:
		Mixer::instance()->updateStream(time);
		ym2413->writeReg(registerLatch, value);
		break;
	default:
		assert(false);
	}
}
