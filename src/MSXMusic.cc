// $Id$

#include "MSXMusic.hh"
#include "MSXMotherBoard.hh"
#include "Mixer.hh"

MSXMusic::MSXMusic(MSXConfig::Device *config) : MSXDevice(config)
{
	PRT_DEBUG("Creating an MSXMusic object");
}

MSXMusic::~MSXMusic()
{
	PRT_DEBUG("Destroying an MSXMusic object");
	delete ym2413;
}

void MSXMusic::init()
{
	MSXDevice::init();
	MSXMotherBoard::instance()->register_IO_Out(0x7c, this);
	MSXMotherBoard::instance()->register_IO_Out(0x7d, this);
	ym2413 = new YM2413();
	loadFile(&romBank, 0x4000);
	reset();
}

void MSXMusic::reset()
{
	ym2413->reset();
	registerLatch = 0; // TODO check
	enable = 1; 	// TODO check
}

void MSXMusic::writeIO(byte port, byte value, EmuTime &time)
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

byte MSXMusic::readMem(word address, EmuTime &time)
{
	switch (address) {
	//case 0x7ff4:	// TODO are these two defined for MSX-MUSIC
	//case 0x7ff5:
	case 0x7ff6:
		return enable;
	//case 0x7ff7:
	default:
		return romBank[address&0x3fff];
	}
}

void MSXMusic::writeMem(word address, byte value, EmuTime &time)
{
	switch (address) {
	//case 0x7ff4:	// TODO are these two defined for MSX-MUSIC
	//case 0x7ff5;
	case 0x7ff6:
		enable = value & 0x11;
		break;
	//case 0x7ff7:
	//default:
		// do nothing
	}
}

void MSXMusic::writeRegisterPort(byte value, EmuTime &time)
{
	registerLatch = (value & 0x3f);
}
void MSXMusic::writeDataPort(byte value, EmuTime &time)
{
	Mixer::instance()->updateStream(time);
	ym2413->writeReg(registerLatch, value);
}
