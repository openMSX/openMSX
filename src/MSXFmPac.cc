// $Id: MSXFmPac.cc,v

#include "MSXFmPac.hh"

//TODO save and restore SRAM 
//TODO check use of enable


MSXFmPac::MSXFmPac(MSXConfig::Device *config) : MSXMusic(config)
{
	PRT_DEBUG("Creating an MSXFmPac object");
	sramBank = new byte[0x1ffe];
}

MSXFmPac::~MSXFmPac()
{
	PRT_DEBUG("Destroying an MSXFmPac object");
	delete[] sramBank;
}

void MSXFmPac::init()
{
	MSXMusic::init();
	loadFile(&romBank, 0x10000);
	reset();
	registerSlots();
}

void MSXFmPac::reset()
{
	MSXMusic::reset();
	sramEnabled = false;
	enable = 1;	// TODO check this
	bank = 0;	// TODO check this
}

byte MSXFmPac::readMem(word address, EmuTime &time)
{
	switch (address) {
	case 0x7ff4:
		// read from YM2413 register port
		return 0xff;
		break;
	case 0x7ff5:
		// read from YM2413 data port
		return 0xff;
		break;
	case 0x7ff6:
		return enable;
		break;
	case 0x7ff7:
		return bank;
		break;
	default:
		if (sramEnabled && (address < 0x5ffe)) {
			return sramBank[address-0x4000];
		} else {
			return romBank[bank*0x4000 + (address-0x4000)];
		}
	}
}

void MSXFmPac::writeMem(word address, byte value, EmuTime &time)
{
	switch (address) {
	case 0x5ffe:
		r5ffe = value;
		checkSramEnable();
		break;
	case 0x5fff:
		r5fff = value;
		checkSramEnable();
		break;
	case 0x7ff4:
		if (enable&1)
			writeRegisterPort(value, time);
		break;
	case 0x7ff5:
		if (enable&1)
			writeDataPort(value, time);
		break;
	case 0x7ff6:
		enable = value&0x11;
		break;
	case 0x7ff7:
		bank = value&0x03;
		break;
	default:
		if (sramEnabled && (address < 0x5ffe)) {
			sramBank[address-0x4000] = value;
		}
	}
}

void MSXFmPac::checkSramEnable()
{
	sramEnabled = ((r5ffe==0x4d)&&(r5fff=0x69)) ? true : false;
}
