// $Id$

#include "MSXFmPac.hh"

#ifndef DONT_WANT_FMPAC

#include "FileOpener.hh"

#include <string.h>

MSXFmPac::MSXFmPac(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXYM2413(config, time), 
	  MSXMemDevice(config, time), MSXRom(config, time)
{
	PRT_DEBUG("Creating an MSXFmPac object");
	sramBank = new byte[0x1ffe];
	try {
		if (deviceConfig->getParameterAsBool("load")) {
			char buffer[16];
			std::string filename = deviceConfig->getParameter("sramname");
			IFILETYPE* file = FileOpener::openFileRO(filename);
			file->read(buffer, 16);
			if (strncmp(PAC_Header, buffer, 16)==0) {
				// correct header
				file->read((char*)sramBank, 0x1ffe);
			}
			file->close();
			delete file;
		}
	} catch (FileOpenerException &e) {
		// do nothing
	}
	loadFile(&memoryBank, 0x10000);
	reset(time);
}

const char* MSXFmPac::PAC_Header = "PAC2 BACKUP DATA";
//                                  1234567890123456

MSXFmPac::~MSXFmPac()
{
	PRT_DEBUG("Destroying an MSXFmPac object");
	if (deviceConfig->getParameterAsBool("save")) {
		std::string filename = deviceConfig->getParameter("sramname");
		IOFILETYPE* file = FileOpener::openFileTruncate(filename);
		file->write(PAC_Header, 16);
		file->write((char*)sramBank, 0x1ffe);
		file->close();
		delete file;
	}
	delete[] sramBank;
}

void MSXFmPac::reset(const EmuTime &time)
{
	MSXYM2413::reset(time);
	sramEnabled = false;
	bank = 0;	// TODO check this
}

byte MSXFmPac::readMem(word address, const EmuTime &time)
{
	switch (address) {
	case 0x7ff4:
		// read from YM2413 register port
		return 0xff;
	case 0x7ff5:
		// read from YM2413 data port
		return 0xff;
	case 0x7ff6:
		return enable;
	case 0x7ff7:
		return bank;
	default:
		address &= 0x3fff;
		if (sramEnabled && (address < 0x1ffe)) {
			return sramBank[address];
		} else {
			return memoryBank[bank*0x4000 + address];
		}
	}
}

void MSXFmPac::writeMem(word address, byte value, const EmuTime &time)
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
		if (enable&1)	// TODO check this
			writeRegisterPort(value, time);
		break;
	case 0x7ff5:
		if (enable&1)	// TODO check this
			writeDataPort(value, time);
		break;
	case 0x7ff6:
		enable = value&0x11;
		break;
	case 0x7ff7:
		bank = value&0x03;
		PRT_DEBUG("FmPac: bank " << (int)bank);
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

#endif // ndef DONT_WANT_FMPAC
