/*
 * 'Ese-SCC' cartride and 'MEGA-SCSI with SCC'(alias WAVE-SCSI) cartridge.
 *
 * Specification:
 *  SRAM(MegaROM) controller: KONAMI_SCC type
 *  SRAM capacity           : 128, 256, 512kB and 1024kb(WAVE-SCSI only)
 *  SCSI Protocol controller: Fujitsu MB89352A
 *
 * ESE-SCC sram write control register:
 *  0x7FFE, 0x7FFF SRAM write control.
 *   bit4       = 1..SRAM writing in 0x4000-0x7FFD can be done.
 *                   It is given priority more than bank changing.
 *              = 0..SRAM read only
 *   other bit  = not used
 *
 * WAVE-SCSI bank control register
 *             6bit register (MA13-MA18, B0-B5)
 *  5000-57FF: 4000-5FFF change
 *  7000-77FF: 6000-7FFF change
 *  9000-97FF: 8000-9FFF change
 *  B000-B7FF: A000-BFFF change
 *
 *  7FFE,7FFF: 2bit register
 *       bit4: bank control MA20 (B7)
 *       bit6: bank control MA19 (B6)
 *  other bit: not used
 *
 * WAVE-SCSI bank map:
 *  00-3F: SRAM read only. mirror of 80-BF.
 *     3F: SCC
 *  40-7F: SPC (MB89352A)
 *  80-FF: SRAM read and write
 *
 * SPC BANK address map:
 * 4000-4FFF spc data register (mirror of 5FFA)
 * 5000-5FEF undefined specification
 * 5FF0-5FFE spc register
 *      5FFF unmapped
 *
 * WAVE-SCSI bank access map (X = accessible)
 *           00-3F 80-BF C0-FF SPC  SCC
 * 4000-5FFF   X     X     X    X    .
 * 6000-7FFF   X     X     .    .    .
 * 8000-9FFF   X     .     .    .    X
 * A000-BFFF   X     .     .    .    .
 */

#include "ESE_SCC.hh"
#include "MB89352.hh"
#include "MSXException.hh"
#include "one_of.hh"
#include "ranges.hh"
#include "serialize.hh"
#include "xrange.hh"
#include <memory>

namespace openmsx {

unsigned ESE_SCC::getSramSize(bool withSCSI) const
{
	unsigned sramSize = getDeviceConfig().getChildDataAsInt("sramsize", 256); // size in kb
	if (sramSize != one_of(1024u, 512u, 256u, 128u)) {
		throw MSXException(
			"SRAM size for ", getName(),
			" should be 128, 256, 512 or 1024kB and not ",
			sramSize, "kB!");
	}
	if (!withSCSI && sramSize == 1024) {
		throw MSXException("1024kB SRAM is only allowed in WAVE-SCSI!");
	}
	return sramSize * 1024; // in bytes
}

ESE_SCC::ESE_SCC(const DeviceConfig& config, bool withSCSI)
	: MSXDevice(config)
	, sram(getName() + " SRAM", getSramSize(withSCSI), config)
	, scc(getName(), config, getCurrentTime())
	, spc(withSCSI ? std::make_unique<MB89352>(config) : nullptr)
	, romBlockDebug(*this, mapper, 0x4000, 0x8000, 13)
	, mapperMask((sram.getSize() / 0x2000) - 1)
	, spcEnable(false)
	, sccEnable(false)
	, writeEnable(false)
{
	// initialized mapper
	ranges::iota(mapper, 0);
}

void ESE_SCC::powerUp(EmuTime::param time)
{
	scc.powerUp(time);
	reset(time);
}

void ESE_SCC::reset(EmuTime::param time)
{
	setMapperHigh(0);
	for (auto i : xrange(4)) {
		setMapperLow(i, i);
	}
	scc.reset(time);
	if (spc) spc->reset(true);
}

void ESE_SCC::setMapperLow(unsigned page, byte value)
{
	value &= 0x3f; // use only 6bit
	bool flush = false;
	if (page == 2) {
		bool newSccEnable = (value == 0x3f);
		if (newSccEnable != sccEnable) {
			sccEnable = newSccEnable;
			flush = true;
		}
	}
	byte newValue = value;
	if (page == 0) newValue |= mapper[0] & 0x40;
	newValue &= mapperMask;
	if (mapper[page] != newValue) {
		mapper[page] = newValue;
		flush = true;
	}
	if (flush) {
		invalidateDeviceRWCache(0x4000 + 0x2000 * page, 0x2000);
	}
}

void ESE_SCC::setMapperHigh(byte value)
{
	writeEnable = (value & 0x10) != 0; // no need to flush cache
	if (!spc) return; // only WAVE-SCSI supports 1024kB

	bool flush = false;
	byte mapperHigh = value & 0x40;
	bool newSpcEnable = mapperHigh && !writeEnable;
	if (spcEnable != newSpcEnable) {
		spcEnable = newSpcEnable;
		flush = true;
	}

	byte newValue = ((mapper[0] & 0x3F) | mapperHigh) & mapperMask;
	if (mapper[0] != newValue) {
		mapper[0] = newValue;
		flush = true;
	}
	if (flush) {
		invalidateDeviceRWCache(0x4000, 0x2000);
	}
}

byte ESE_SCC::readMem(word address, EmuTime::param time)
{
	unsigned page = address / 0x2000 - 2;
	// SPC
	if (spcEnable && (page == 0)) {
		address &= 0x1fff;
		if (address < 0x1000) {
			return spc->readDREG();
		} else {
			return spc->readRegister(address & 0x0f);
		}
	}
	// SCC bank
	if (sccEnable && (address >= 0x9800) && (address < 0xa000)) {
		return scc.readMem(address & 0xff, time);
	}
	// SRAM read
	return sram[mapper[page] * 0x2000 + (address & 0x1fff)];
}

byte ESE_SCC::peekMem(word address, EmuTime::param time) const
{
	unsigned page = address / 0x2000 - 2;
	// SPC
	if (spcEnable && (page == 0)) {
		address &= 0x1fff;
		if (address < 0x1000) {
			return spc->peekDREG();
		} else {
			return spc->peekRegister(address & 0x0f);
		}
	}
	// SCC bank
	if (sccEnable && (address >= 0x9800) && (address < 0xa000)) {
		return scc.peekMem(address & 0xff, time);
	}
	// SRAM read
	return sram[mapper[page] * 0x2000 + (address & 0x1fff)];
}

const byte* ESE_SCC::getReadCacheLine(word address) const
{
	unsigned page = address / 0x2000 - 2;
	// SPC
	if (spcEnable && (page == 0)) {
		return nullptr;
	}
	// SCC bank
	if (sccEnable && (address >= 0x9800) && (address < 0xa000)) {
		return nullptr;
	}
	// SRAM read
	return &sram[mapper[page] * 0x2000 + (address & 0x1fff)];
}

void ESE_SCC::writeMem(word address, byte value, EmuTime::param time)
{
	unsigned page = address / 0x2000 - 2;
	// SPC Write
	if (spcEnable && (page == 0)) {
		address &= 0x1fff;
		if (address < 0x1000) {
			spc->writeDREG(value);
		} else {
			spc->writeRegister(address & 0x0f, value);
		}
		return;
	}

	// SCC write
	if (sccEnable && (0x9800 <= address) && (address < 0xa000)) {
		scc.writeMem(address & 0xff, value, time);
		return;
	}

	// set mapper high control register
	if ((address | 0x0001) == 0x7FFF) {
		setMapperHigh(value);
		return;
	}

	// SRAM write (processing of 0x4000-0x7FFD)
	if (writeEnable && (page < 2)) {
		sram.write(mapper[page] * 0x2000 + (address & 0x1FFF), value);
		return;
	}

	// Bank change
	if (((address & 0x1800) == 0x1000)) {
		setMapperLow(page, value);
		return;
	}
}

byte* ESE_SCC::getWriteCacheLine(word /*address*/) const
{
	return nullptr; // not cacheable
}


template<typename Archive>
void ESE_SCC::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("sram", sram,
	             "scc",  scc);
	if (spc) ar.serialize("MB89352", *spc);
	ar.serialize("mapper",      mapper,
	             "spcEnable",   spcEnable,
	             "sccEnable",   sccEnable,
	             "writeEnable", writeEnable);
}
INSTANTIATE_SERIALIZE_METHODS(ESE_SCC);
REGISTER_MSXDEVICE(ESE_SCC, "ESE_SCC");

} // namespace openmsx
