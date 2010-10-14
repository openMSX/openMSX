// $Id$

#include "RomManbow2.hh"
#include "Rom.hh"
#include "SCC.hh"
#include "AmdFlash.hh"
#include "serialize.hh"
#include <cassert>
#include <vector>

namespace openmsx {

static unsigned getWriteProtected(RomType type)
{
	assert((type == ROM_MANBOW2) || (type == ROM_MEGAFLASHROMSCC));
	return (type == ROM_MANBOW2) ? 0x7F  // only last 64kb is writeable
	                             : 0x00; // fully writeable
}

RomManbow2::RomManbow2(MSXMotherBoard& motherBoard, const XMLElement& config,
                       std::auto_ptr<Rom> rom_, RomType type)
	: MSXRom(motherBoard, config, rom_)
	, scc(new SCC(motherBoard, "SCC", config, getCurrentTime()))
	, flash(new AmdFlash(motherBoard, *rom,
	                     std::vector<unsigned>(512 / 64, 0x10000),
	                     getWriteProtected(type), 0x01A4, config))
{
	powerUp(getCurrentTime());
}

RomManbow2::~RomManbow2()
{
}

void RomManbow2::powerUp(EmuTime::param time)
{
	scc->powerUp(time);
	reset(time);
}

void RomManbow2::reset(EmuTime::param time)
{
	for (int i = 0; i < 4; i++) {
		setRom(i, i);
	}

	sccEnabled = false;
	scc->reset(time);

	flash->reset();
}

void RomManbow2::setRom(unsigned region, unsigned block)
{
	assert(region < 4);
	unsigned nrBlocks = flash->getSize() / 0x2000;
	bank[region] = block & (nrBlocks - 1);
	invalidateMemCache(0x4000 + region * 0x2000, 0x2000);
}

byte RomManbow2::peekMem(word address, EmuTime::param time) const
{
	if (sccEnabled && (0x9800 <= address) && (address < 0xA000)) {
		return scc->peekMem(address & 0xFF, time);
	} else if ((0x4000 <= address) && (address < 0xC000)) {
		unsigned page = (address - 0x4000) / 0x2000;
		unsigned addr = (address & 0x1FFF) + 0x2000 * bank[page];
		return flash->peek(addr);
	} else {
		return 0xFF;
	}
}

byte RomManbow2::readMem(word address, EmuTime::param time)
{
	if (sccEnabled && (0x9800 <= address) && (address < 0xA000)) {
		return scc->readMem(address & 0xFF, time);
	} else if ((0x4000 <= address) && (address < 0xC000)) {
		unsigned page = (address - 0x4000) / 0x2000;
		unsigned addr = (address & 0x1FFF) + 0x2000 * bank[page];
		return flash->read(addr);
	} else {
		return 0xFF;
	}
}

const byte* RomManbow2::getReadCacheLine(word address) const
{
	if (sccEnabled && (0x9800 <= address) && (address < 0xA000)) {
		return NULL;
	} else if ((0x4000 <= address) && (address < 0xC000)) {
		unsigned page = (address - 0x4000) / 0x2000;
		unsigned addr = (address & 0x1FFF) + 0x2000 * bank[page];
		return flash->getReadCacheLine(addr);
	} else {
		return unmappedRead;
	}
}

void RomManbow2::writeMem(word address, byte value, EmuTime::param time)
{
	if (sccEnabled && (0x9800 <= address) && (address < 0xA000)) {
		// write to SCC
		scc->writeMem(address & 0xff, value, time);
		// note: writes to SCC also go to flash
		//    thanks to 'enen' for testing this
	}
	if ((0x4000 <= address) && (address < 0xC000)) {
		unsigned page = (address - 0x4000) / 0x2000;
		unsigned addr = (address & 0x1FFF) + 0x2000 * bank[page];
		flash->write(addr, value);

		if ((address & 0xF800) == 0x9000) {
			// SCC enable/disable
			sccEnabled = ((value & 0x3F) == 0x3F);
			invalidateMemCache(0x9800, 0x0800);
		}
		if ((address & 0x1800) == 0x1000) {
			// page selection
			setRom(page, value);
		}
	}
}

byte* RomManbow2::getWriteCacheLine(word address) const
{
	if ((0x4000 <= address) && (address < 0xC000)) {
		return NULL;
	} else {
		return unmappedWrite;
	}
}


template<typename Archive>
void RomManbow2::serialize(Archive& ar, unsigned /*version*/)
{
	// skip MSXRom base class
	ar.template serializeBase<MSXDevice>(*this);

	ar.serialize("scc", *scc);
	ar.serialize("flash", *flash);
	ar.serialize("bank", bank);
	ar.serialize("sccEnabled", sccEnabled);
}
INSTANTIATE_SERIALIZE_METHODS(RomManbow2);
REGISTER_MSXDEVICE(RomManbow2, "RomManbow2");

} // namespace openmsx
