// $Id$

#include "MSXFmPac.hh"
#include "MSXConfig.hh"
#include "MSXCPU.hh"
#include "CPU.hh"


static const char* PAC_Header = "PAC2 BACKUP DATA";
//                               1234567890123456

MSXFmPac::MSXFmPac(Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXMusic(config, time), 
	  sram(0x1FFE, config, PAC_Header)
{
	reset(time);
}

MSXFmPac::~MSXFmPac()
{
}

void MSXFmPac::reset(const EmuTime &time)
{
	MSXMusic::reset(time);
	enable = 0;
	sramEnabled = false;
	bank = 0;	// TODO check this
}

void MSXFmPac::writeIO(byte port, byte value, const EmuTime &time)
{
	if (enable & 1) {
		MSXMusic::writeIO(port, value, time);
	}
}

byte MSXFmPac::readMem(word address, const EmuTime &time)
{
	switch (address) {
		case 0x7FF4:
			// read from YM2413 register port
			return 0xFF;
		case 0x7FF5:
			// read from YM2413 data port
			return 0xFF;
		case 0x7FF6:
			return enable;
		case 0x7FF7:
			return bank;
		default:
			address &= 0x3FFF;
			if (sramEnabled && (address < 0x1FFE)) {
				return sram.read(address);
			} else {
				return rom.read(bank * 0x4000 + address);
			}
	}
}

const byte* MSXFmPac::getReadCacheLine(word address) const
{
	if (address == (0x7FF4 & CPU::CACHE_LINE_HIGH)) {
		return NULL;
	}
	address &= 0x3FFF;
	if (address == (0x1FFE & CPU::CACHE_LINE_HIGH)) {
		return NULL;
	}
	if (sramEnabled && (address < 0x1FFE)) {
		return sram.getBlock(address);
	} else {
		return rom.getBlock(bank * 0x4000 + address);
	}
}

void MSXFmPac::writeMem(word address, byte value, const EmuTime &time)
{
	switch (address) {
		case 0x5FFE:
			r5ffe = value;
			checkSramEnable();
			break;
		case 0x5FFF:
			r5fff = value;
			checkSramEnable();
			break;
		case 0x7FF4:
			if (enable & 1)	// TODO check this
				writeRegisterPort(value, time);
			break;
		case 0x7FF5:
			if (enable & 1)	// TODO check this
				writeDataPort(value, time);
			break;
		case 0x7FF6:
			enable = value & 0x11;
			break;
		case 0x7FF7: {
			byte newBank = value & 0x03;
			if (bank != newBank) {
				bank = newBank;
				MSXCPU::instance()->invalidateCache(0x0000,
				              0x10000 / CPU::CACHE_LINE_SIZE);
			}
			break;
		}
		default:
			if (sramEnabled && (address < 0x5FFE))
				sram.write(address - 0x4000, value);
	}
}

byte* MSXFmPac::getWriteCacheLine(word address) const
{
	if (address == (0x5FFE & CPU::CACHE_LINE_HIGH)) {
		return NULL;
	}
	if (address == (0x7FF4 & CPU::CACHE_LINE_HIGH)) {
		return NULL;
	}
	if (sramEnabled && (address < 0x5FFE)) {
		return sram.getBlock(address - 0x4000);
	} else {
		return unmappedWrite;
	}
}

void MSXFmPac::checkSramEnable()
{
	bool newEnabled = ((r5ffe == 0x4D) && (r5fff == 0x69)) ? true : false;
	if (sramEnabled != newEnabled) {
		sramEnabled = newEnabled;
		MSXCPU::instance()->invalidateCache(0x0000,
		                              0x10000 / CPU::CACHE_LINE_SIZE);
	}
}

