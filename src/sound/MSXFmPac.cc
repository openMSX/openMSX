// $Id$

#include "MSXFmPac.hh"
#include "SRAM.hh"
#include "Rom.hh"
#include "MSXCPU.hh"
#include "MSXMotherBoard.hh"
#include "CacheLine.hh"

namespace openmsx {

static const char* const PAC_Header = "PAC2 BACKUP DATA";

MSXFmPac::MSXFmPac(MSXMotherBoard& motherBoard, const XMLElement& config,
                   const EmuTime& time)
	: MSXMusic(motherBoard, config, time)
	, sram(new SRAM(motherBoard, getName() + " SRAM", 0x1FFE, config,
	                PAC_Header))
{
	reset(time);
}

MSXFmPac::~MSXFmPac()
{
}

void MSXFmPac::reset(const EmuTime& time)
{
	MSXMusic::reset(time);
	enable = 0;
	sramEnabled = false;
	bank = 0;
	r1ffe = r1fff = 0; // actual value doesn't matter as long
	                   // as it's not the magic combination
}

void MSXFmPac::writeIO(word port, byte value, const EmuTime& time)
{
	if (enable & 1) {
		MSXMusic::writeIO(port, value, time);
	}
}

byte MSXFmPac::readMem(word address, const EmuTime& /*time*/)
{
	address &= 0x3FFF;
	switch (address) {
		case 0x3FF6:
			return enable;
		case 0x3FF7:
			return bank;
		default:
			if (sramEnabled) {
				if (address < 0x1FFE) {
					return (*sram)[address];
				} else if (address == 0x1FFE) {
					return r1ffe; // always 0x4D
				} else if (address == 0x1FFF) {
					return r1fff; // always 0x69
				} else {
					return 0xFF;
				}
			} else {
				return (*rom)[bank * 0x4000 + address];
			}
	}
}

const byte* MSXFmPac::getReadCacheLine(word address) const
{
	address &= 0x3FFF;
	if (address == (0x3FF6 & CacheLine::HIGH)) {
		return NULL;
	}
	if (sramEnabled) {
		if (address < (0x1FFE & CacheLine::HIGH)) {
			return &(*sram)[address];
		} else if (address == (0x1FFE & CacheLine::HIGH)) {
			return NULL;
		} else {
			return unmappedRead;
		}
	} else {
		return &(*rom)[bank * 0x4000 + address];
	}
}

void MSXFmPac::writeMem(word address, byte value, const EmuTime& time)
{
	// 'enable' has no effect for memory mapped access
	//   (thanks to BiFiMSX for investigating this)
	address &= 0x3FFF;
	switch (address) {
		case 0x1FFE:
			if (!(enable & 0x10)) {
				r1ffe = value;
				checkSramEnable();
			}
			break;
		case 0x1FFF:
			if (!(enable & 0x10)) {
				r1fff = value;
				checkSramEnable();
			}
			break;
		case 0x3FF4:
			writeRegisterPort(value, time);
			break;
		case 0x3FF5:
			writeDataPort(value, time);
			break;
		case 0x3FF6:
			enable = value & 0x11;
			if (enable & 0x10) {
				r1ffe = r1fff = 0; // actual value not important
				checkSramEnable();
			}
			break;
		case 0x3FF7: {
			byte newBank = value & 0x03;
			if (bank != newBank) {
				bank = newBank;
				getMotherBoard().getCPU().invalidateMemCache(
					0x0000, 0x10000);
			}
			break;
		}
		default:
			if (sramEnabled && (address < 0x1FFE)) {
				sram->write(address, value);
			}
	}
}

byte* MSXFmPac::getWriteCacheLine(word address) const
{
	address &= 0x3FFF;
	if (address == (0x1FFE & CacheLine::HIGH)) {
		return NULL;
	}
	if (address == (0x3FF4 & CacheLine::HIGH)) {
		return NULL;
	}
	if (sramEnabled && (address < 0x1FFE)) {
		return NULL;
	} else {
		return unmappedWrite;
	}
}

void MSXFmPac::checkSramEnable()
{
	bool newEnabled = (r1ffe == 0x4D) && (r1fff == 0x69);
	if (sramEnabled != newEnabled) {
		sramEnabled = newEnabled;
		getMotherBoard().getCPU().invalidateMemCache(0x0000, 0x10000);
	}
}

} // namespace openmsx
