// $Id$

#include "MSXBunsetsu.hh"
#include "CPU.hh"
#include "MSXConfig.hh"


MSXBunsetsu::MSXBunsetsu(Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXMemDevice(config, time),
	  rom(config, time),
	  jisyoRom(config, config->getParameter("jisyofilename"), time)
{
	reset(time);
}

MSXBunsetsu::~MSXBunsetsu()
{
}

void MSXBunsetsu::reset(const EmuTime &time)
{
	jisyoAddress = 0;
}

byte MSXBunsetsu::readMem(word address, const EmuTime &time)
{
	byte result;
	if (address == 0xBFFF) {
		result = jisyoRom.read(jisyoAddress);
		jisyoAddress = (jisyoAddress + 1) & 0x1FFFF;
	} else if ((0x4000 <= address) && (address < 0xC000)) {
		result = rom.read(address - 0x4000);
	} else {
		result = 0xFF;
	}
	return result;
}

void MSXBunsetsu::writeMem(word address, byte value, const EmuTime &time)
{
	switch (address) {
		case 0xBFFC:
			jisyoAddress = (jisyoAddress & 0x1FF00) | value;
			break;
		case 0xBFFD:
			jisyoAddress = (jisyoAddress & 0x100FF) | (value << 8);
			break;
		case 0xBFFE:
			jisyoAddress = (jisyoAddress & 0x0FFFF) | 
			               ((value & 1) << 16);
			break;
	}
}

const byte* MSXBunsetsu::getReadCacheLine(word start) const
{
	if ((start & CPU::CACHE_LINE_HIGH) == 
	    (0xBFFF & CPU::CACHE_LINE_HIGH)) {
		return NULL;
	} else {
		return rom.getBlock(start - 0x4000);
	}
}
