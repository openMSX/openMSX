// $Id$

#include "MSXMusic.hh"

#ifndef DONT_WANT_MSXMUSIC

MSXMusic::MSXMusic(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXYM2413(config, time),
	  MSXMemDevice(config, time), MSXRomDevice(config, time)
{
	PRT_DEBUG("Creating an MSXMusic object");
	loadFile(&memoryBank, 0x4000);
	enable = 1;
}

MSXMusic::~MSXMusic()
{
	PRT_DEBUG("Destroying an MSXMusic object");
}

void MSXMusic::reset(const EmuTime &time)
{
	MSXDevice::reset(time);
	MSXYM2413::reset(time);
}

byte MSXMusic::readMem(word address, const EmuTime &time)
{
	switch (address) {
	//case 0x7ff4:	// TODO are these two defined for MSX-MUSIC
	//case 0x7ff5:
	case 0x7ff6:
		return enable;
	//case 0x7ff7:
	default:
		return memoryBank[address&0x3fff];
	}
}

void MSXMusic::writeMem(word address, byte value, const EmuTime &time)
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

#endif // ndef DONT_WANT_MSXMUSIC
