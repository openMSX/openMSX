// $Id$

#include "MSXPac.hh"
#include "MSXCPU.hh"
#include "CPU.hh"

namespace openmsx {

static const char* const PAC_Header = "PAC2 BACKUP DATA";
//                                     1234567890123456

MSXPac::MSXPac(const XMLElement& config, const EmuTime& time)
	: MSXDevice(config, time), MSXMemDevice(config, time), 
	  sram(getName() + " SRAM", 0x1FFE, config, PAC_Header)
{
	reset(time);
}

MSXPac::~MSXPac()
{
}

void MSXPac::reset(const EmuTime& /*time*/)
{
	sramEnabled = false;
	r1ffe = r1fff = 0xFF;	// TODO check
}

byte MSXPac::readMem(word address, const EmuTime& /*time*/)
{
	byte result;
	address &= 0x3FFF;
	if (sramEnabled) {
		if (address < 0x1FFE) {
			result = sram[address];
		} else if (address == 0x1FFE) {
			result = r1ffe;
		} else if (address == 0x1FFF) {
			result = r1fff;
		} else {
			result = 0xFF;
		}
	} else {
		result = 0xFF;
	}
	//PRT_DEBUG("PAC read "<<hex<<(int)address<<" "<<(int)result<<dec);
	return result;
}

const byte* MSXPac::getReadCacheLine(word address) const
{
	address &= 0x3FFF;
	if (sramEnabled) {
		if (address < (0x1FFE & CPU::CACHE_LINE_HIGH)) {
			return &sram[address];
		} else if (address == (0x1FFE & CPU::CACHE_LINE_HIGH)) {
			return NULL;
		} else {
			return unmappedRead;
		}
	} else {
		return unmappedRead;
	}
}

void MSXPac::writeMem(word address, byte value, const EmuTime& /*time*/)
{
	//PRT_DEBUG("PAC write "<<hex<<(int)address<<" "<<(int)value<<dec);
	address &= 0x3FFF;
	switch (address) {
		case 0x1FFE:
			r1ffe = value;
			checkSramEnable();
			break;
		case 0x1FFF:
			r1fff = value;
			checkSramEnable();
			break;
		default:
			if (sramEnabled && (address < 0x1FFE)) {
				sram[address] = value;
			}
	}
}

byte* MSXPac::getWriteCacheLine(word address) const
{
	address &= 0x3FFF;
	if (address == (0x1FFE & CPU::CACHE_LINE_HIGH)) {
		return NULL;
	}
	if (sramEnabled && (address < 0x1FFE)) {
		return const_cast<byte*>(&sram[address]);
	} else {
		return unmappedWrite;
	}
}

void MSXPac::checkSramEnable()
{
	bool newEnabled = (r1ffe == 0x4D) && (r1fff == 0x69);
	if (sramEnabled != newEnabled) {
		sramEnabled = newEnabled;
		MSXCPU::instance().invalidateCache(0x0000,
		                              0x10000 / CPU::CACHE_LINE_SIZE);
	}
}

} // namespace openmsx
