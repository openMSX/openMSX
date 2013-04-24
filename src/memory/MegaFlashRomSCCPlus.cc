#include "MegaFlashRomSCCPlus.hh"
#include "Rom.hh"
#include "SCC.hh"
#include "AY8910.hh"
#include "DummyAY8910Periphery.hh"
#include "AmdFlash.hh"
#include "MSXCPUInterface.hh"
#include "CacheLine.hh"
#include "serialize.hh"
#include "memory.hh"
#include <cassert>
#include <vector>

namespace openmsx {

static unsigned sectorSizes[19] = {
	0x4000, 0x2000, 0x2000, 0x8000,      // 16kB + 8kB + 8kB + 32kB
	0x10000, 0x10000, 0x10000, 0x10000,  // 15 * 64kB
	0x10000, 0x10000, 0x10000, 0x10000,
	0x10000, 0x10000, 0x10000, 0x10000,
	0x10000, 0x10000, 0x10000,
};

MegaFlashRomSCCPlus::MegaFlashRomSCCPlus(
		const DeviceConfig& config, std::unique_ptr<Rom> rom_)
	: MSXRom(config, std::move(rom_))
	, scc(make_unique<SCC>(
		"MFR SCC+ SCC-I", config, getCurrentTime(),
		SCC::SCC_Compatible))
	, psg(make_unique<AY8910>(
		"MFR SCC+ PSG", DummyAY8910Periphery::instance(), config,
		getCurrentTime()))
	, flash(make_unique<AmdFlash>(
		*rom, std::vector<unsigned>(sectorSizes, sectorSizes + 19),
		0, 0x205B, config))
{
	powerUp(getCurrentTime());

	getCPUInterface().register_IO_Out(0x10, this);
	getCPUInterface().register_IO_Out(0x11, this);
	getCPUInterface().register_IO_In (0x12, this);
}

MegaFlashRomSCCPlus::~MegaFlashRomSCCPlus()
{
	getCPUInterface().unregister_IO_Out(0x10, this);
	getCPUInterface().unregister_IO_Out(0x11, this);
	getCPUInterface().unregister_IO_In (0x12, this);
}

void MegaFlashRomSCCPlus::powerUp(EmuTime::param time)
{
	scc->powerUp(time);
	reset(time);
}

void MegaFlashRomSCCPlus::reset(EmuTime::param time)
{
	configReg = 0;
	offsetReg = 0;
	subslotReg = 0;
	for (int subslot = 0; subslot < 4; ++subslot) {
		for (int bank = 0; bank < 4; ++bank) {
			bankRegs[subslot][bank] = bank;
		}
	}

	sccMode = 0;
	for (int i = 0; i < 4; ++i) {
		sccBanks[i] = i;
	}
	scc->reset(time);

	psgLatch = 0;
	psg->reset(time);

	flash->reset();
}

MegaFlashRomSCCPlus::SCCEnable MegaFlashRomSCCPlus::getSCCEnable() const
{
	if ((sccMode & 0x20) && (sccBanks[3] & 0x80)) {
		return EN_SCCPLUS;
	} else if ((!(sccMode & 0x20)) && ((sccBanks[2] & 0x3F) == 0x3F)) {
		return EN_SCC;
	} else {
		return EN_NONE;
	}
}

unsigned MegaFlashRomSCCPlus::getSubslot(unsigned addr) const
{
	return (configReg & 0x10)
	     ? (subslotReg >> (2 * (addr >> 14))) & 0x03
	     : 0;
}

unsigned MegaFlashRomSCCPlus::getFlashAddr(unsigned addr) const
{
	unsigned subslot = getSubslot(addr);
	unsigned tmp;
	if ((configReg & 0xC0) == 0x40) {
		unsigned bank = bankRegs[subslot][addr >> 14] + offsetReg;
		tmp = (bank * 0x4000) + (addr & 0x3FFF);
	} else {
		unsigned page = (addr >> 13) - 2;
		if (page >= 4) {
			// Bank: -2, -1, 4, 5. So not mapped in this region,
			// returned value should not be used. But querying it
			// anyway is easier, see start of writeMem().
			return unsigned(-1);
		}
		unsigned bank = bankRegs[subslot][page] + offsetReg;
		tmp = (bank * 0x2000) + (addr & 0x1FFF);
	}
	return ((0x40000 * subslot) + tmp) & 0xFFFFF; // wrap at 1MB
}

byte MegaFlashRomSCCPlus::peekMem(word addr, EmuTime::param time) const
{
	if ((configReg & 0x10) && (addr == 0xFFFF)) {
		// read subslot register
		return subslotReg ^ 0xFF;
	}

	if ((configReg & 0xE0) == 0x00) {
		SCCEnable enable = getSCCEnable();
		if (((enable == EN_SCC)     && (0x9800 <= addr) && (addr < 0xA000)) ||
		    ((enable == EN_SCCPLUS) && (0xB800 <= addr) && (addr < 0xC000))) {
			return scc->peekMem(addr & 0xFF, time);
		}
	}

	if (((configReg & 0xC0) == 0x40) ||
	    ((0x4000 <= addr) && (addr < 0xC000))) {
		// read (flash)rom content
		unsigned flashAddr = getFlashAddr(addr);
		assert(flashAddr != unsigned(-1));
		return flash->peek(flashAddr);
	} else {
		// unmapped read
		return 0xFF;
	}
}

byte MegaFlashRomSCCPlus::readMem(word addr, EmuTime::param time)
{
	if ((configReg & 0x10) && (addr == 0xFFFF)) {
		// read subslot register
		return subslotReg ^ 0xFF;
	}

	if ((configReg & 0xE0) == 0x00) {
		SCCEnable enable = getSCCEnable();
		if (((enable == EN_SCC)     && (0x9800 <= addr) && (addr < 0xA000)) ||
		    ((enable == EN_SCCPLUS) && (0xB800 <= addr) && (addr < 0xC000))) {
			return scc->readMem(addr & 0xFF, time);
		}
	}

	if (((configReg & 0xC0) == 0x40) ||
	    ((0x4000 <= addr) && (addr < 0xC000))) {
		// read (flash)rom content
		unsigned flashAddr = getFlashAddr(addr);
		assert(flashAddr != unsigned(-1));
		return flash->read(flashAddr);
	} else {
		// unmapped read
		return 0xFF;
	}
}

const byte* MegaFlashRomSCCPlus::getReadCacheLine(word addr) const
{
	if ((configReg & 0x10) &&
	    ((addr & CacheLine::HIGH) == (0xFFFF & CacheLine::HIGH))) {
		// read subslot register
		return nullptr;
	}

	if ((configReg & 0xE0) == 0x00) {
		SCCEnable enable = getSCCEnable();
		if (((enable == EN_SCC)     && (0x9800 <= addr) && (addr < 0xA000)) ||
		    ((enable == EN_SCCPLUS) && (0xB800 <= addr) && (addr < 0xC000))) {
			return nullptr;
		}
	}

	if (((configReg & 0xC0) == 0x40) ||
	    ((0x4000 <= addr) && (addr < 0xC000))) {
		// read (flash)rom content
		unsigned flashAddr = getFlashAddr(addr);
		assert(flashAddr != unsigned(-1));
		return flash->getReadCacheLine(flashAddr);
	} else {
		// unmapped read
		return unmappedRead;
	}
}

void MegaFlashRomSCCPlus::writeMem(word addr, byte value, EmuTime::param time)
{
	// address is calculated before writes to other regions take effect
	unsigned flashAddr = getFlashAddr(addr);

	// There are several overlapping functional regions in the address
	// space. A single write can trigger behaviour in multiple regions. In
	// other words there's no priority amongst the regions where a higher
	// priority region blocks the write from the lower priority regions.
	if ((configReg & 0x10) && (addr == 0xFFFF)) {
		// write subslot register
		byte diff = value ^ subslotReg;
		subslotReg = value;
		for (int i = 0; i < 4; ++i) {
			if (diff & (3 << (2 * i))) {
				invalidateMemCache(0x4000 * i, 0x4000);
			}
		}
	}

	if (((configReg & 0x04) == 0x00) && ((addr & 0xFFFE) == 0x7FFE)) {
		// write config register
		configReg = value;
		invalidateMemCache(0x0000, 0x10000); // flush all to be sure
	}

	if ((configReg & 0xE0) == 0x00) {
		// Konami-SCC
		if ((addr & 0xFFFE) == 0xBFFE) {
			sccMode = value;
			scc->setChipMode((value & 0x20) ? SCC::SCC_plusmode
			                                : SCC::SCC_Compatible);
		}
		SCCEnable enable = getSCCEnable();
		bool isRamSegment2 = ((sccMode & 0x24) == 0x24) ||
		                     ((sccMode & 0x10) == 0x10);
		bool isRamSegment3 = ((sccMode & 0x10) == 0x10);
		if (((enable == EN_SCC)     && !isRamSegment2 &&
		     (0x9800 <= addr) && (addr < 0xA000)) ||
		    ((enable == EN_SCCPLUS) && !isRamSegment3 &&
		     (0xB800 <= addr) && (addr < 0xC000))) {
			scc->writeMem(addr & 0xFF, value, time);
		}
	}

	unsigned subslot = getSubslot(addr);
	unsigned page8 = (addr >> 13) - 2;
	if (((configReg & 0x02) == 0x00) && (page8 < 4)) {
		// (possibly) write to bank registers
		switch (configReg & 0xE0) {
		case 0x00:
			// Konami-SCC
			if ((addr & 0x1800) == 0x1000) {
				// Storing 'sccBanks' may seem redundant at
				// first, but it's required to calculate
				// whether the SCC is enabled or not.
				sccBanks[page8] = value;
				if ((value & 0x80) && (page8 == 0)) {
					offsetReg = value & 0x7F;
					invalidateMemCache(0x4000, 0x8000);
				} else {
					// Making of the mapper bits is done on
					// write (and only in Konami(-scc) mode)
					byte mask = (configReg & 0x01) ? 0x3F : 0x7F;
					bankRegs[subslot][page8] = value & mask;
					invalidateMemCache(0x4000 + 0x2000 * page8, 0x2000);
				}
			}
			break;
		case 0x20: {
			// Konami
			if (((configReg & 0x08) == 0x08) && (addr < 0x6000)) {
				// Switching 0x4000-0x5FFF disabled.
				// This bit blocks writing to the bank register
				// (an alternative was forcing a 0 on read).
				// It only has effect in Konami mode.
				break;
			}
			// Making of the mapper bits is done on
			// write (and only in Konami(-scc) mode)
			if ((addr < 0x5000) || ((0x5800 <= addr) && (addr < 0x6000))) break; // only SCC range works
			byte mask = (configReg & 0x01) ? 0x1F : 0x7F;
			bankRegs[subslot][page8] = value & mask;
			invalidateMemCache(0x4000 + 0x2000 * page8, 0x2000);
			break;
		}
		case 0x40:
		case 0x60:
			// 64kB
			bankRegs[subslot][page8] = value;
			invalidateMemCache(0x4000 + 0x2000 * page8, 0x2000);
			break;
		case 0x80:
		case 0xA0:
			// ASCII-8
			if ((0x6000 <= addr) && (addr < 0x8000)) {
				byte bank = (addr >> 11) & 0x03;
				bankRegs[subslot][bank] = value;
				invalidateMemCache(0x4000 + 0x2000 * page8, 0x2000);
			}
			break;
		case 0xC0:
		case 0xE0:
			// ASCII-16
			// This behaviour is confirmed by Manuel Pazos (creator
			// of the cartridge): ASCII-16 uses all 4 bank registers
			// and one bank switch changes 2 registers at once.
			// This matters when switching mapper mode, because
			// the content of the bank registers is unchanged after
			// a switch.
			if ((0x6000 <= addr) && (addr < 0x6800)) {
				bankRegs[subslot][0] = 2 * value + 0;
				bankRegs[subslot][1] = 2 * value + 1;
				invalidateMemCache(0x4000, 0x4000);
			}
			if ((0x7000 <= addr) && (addr < 0x7800)) {
				bankRegs[subslot][2] = 2 * value + 0;
				bankRegs[subslot][3] = 2 * value + 1;
				invalidateMemCache(0x8000, 0x4000);
			}
			break;
		}
	}

	// write to flash
	if (((configReg & 0xC0) == 0x40) ||
	    ((0x4000 <= addr) && (addr < 0xC000))) {
		assert(flashAddr != unsigned(-1));
		return flash->write(flashAddr, value);
	}
}

byte* MegaFlashRomSCCPlus::getWriteCacheLine(word /*addr*/) const
{
	return nullptr;
}


byte MegaFlashRomSCCPlus::readIO(word port, EmuTime::param time)
{
	assert((port & 0xFF) == 0x12); (void)port;
	return psg->readRegister(psgLatch, time);
}

byte MegaFlashRomSCCPlus::peekIO(word port, EmuTime::param time) const
{
	assert((port & 0xFF) == 0x12); (void)port;
	return psg->peekRegister(psgLatch, time);
}

void MegaFlashRomSCCPlus::writeIO(word port, byte value, EmuTime::param time)
{
	if ((port & 0xFF) == 0x10) {
		psgLatch = value & 0x0F;
	} else {
		assert((port & 0xFF) == 0x11);
		psg->writeRegister(psgLatch, value, time);
	}
}


template<typename Archive>
void MegaFlashRomSCCPlus::serialize(Archive& ar, unsigned /*version*/)
{
	// skip MSXRom base class
	ar.template serializeBase<MSXDevice>(*this);

	ar.serialize("scc", *scc);
	ar.serialize("psg", *psg);
	ar.serialize("flash", *flash);

	ar.serialize("configReg", configReg);
	ar.serialize("offsetReg", offsetReg);
	ar.serialize("subslotReg", subslotReg);
	ar.serialize("bankRegs", bankRegs);
	ar.serialize("psgLatch", psgLatch);
	ar.serialize("sccMode", sccMode);
	ar.serialize("sccBanks", sccBanks);
}
INSTANTIATE_SERIALIZE_METHODS(MegaFlashRomSCCPlus);
REGISTER_MSXDEVICE(MegaFlashRomSCCPlus, "MegaFlashRomSCCPlus");

} // namespace openmsx
