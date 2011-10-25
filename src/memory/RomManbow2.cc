// $Id$

#include "RomManbow2.hh"
#include "Rom.hh"
#include "SCC.hh"
#include "AY8910.hh"
#include "DummyAY8910Periphery.hh"
#include "MSXCPUInterface.hh"
#include "MSXMotherBoard.hh"
#include "AmdFlash.hh"
#include "serialize.hh"
#include <cassert>
#include <vector>

using std::string;

namespace openmsx {

static unsigned getWriteProtected(RomType type)
{
	assert((type == ROM_MANBOW2) || (type == ROM_MEGAFLASHROMSCC) || (type == ROM_MANBOW2_2) || (type == ROM_HAMARAJANIGHT));
	switch (type) {
	case ROM_MANBOW2:
	case ROM_MANBOW2_2:
		return 0x7F; // only the last 64kb is writeable
	case ROM_HAMARAJANIGHT:
		return 0xCF; // only 128kb is writeable
	default:
		return 0x00; // fully writeable
	}
}

static string getNameFrom(RomType type)
{
	switch (type) {
	case ROM_MANBOW2:
		return "";
	case ROM_MANBOW2_2:
		return "Manbow 2 ";
	case ROM_HAMARAJANIGHT:
		return "Hamaraja Nights ";
	default:
		return "";
	}
}


RomManbow2::RomManbow2(MSXMotherBoard& motherBoard, const XMLElement& config,
                       std::auto_ptr<Rom> rom_, RomType type)
	: MSXRom(motherBoard, config, rom_)
	, scc(new SCC(motherBoard, getNameFrom(type) + "SCC", config, getCurrentTime()))
	, psg(((type == ROM_MANBOW2_2) || (type == ROM_HAMARAJANIGHT)) ?
	      new AY8910(motherBoard, getNameFrom(type) + "PSG", DummyAY8910Periphery::instance(), config,
			 getCurrentTime()) : NULL)
	, flash(new AmdFlash(motherBoard, *rom,
	                     std::vector<unsigned>(512 / 64, 0x10000),
	                     getWriteProtected(type), 0x01A4, config))
{
	powerUp(getCurrentTime());

	if (psg.get()) {
		getMotherBoard().getCPUInterface().register_IO_Out(0x10, this);
		getMotherBoard().getCPUInterface().register_IO_Out(0x11, this);
		getMotherBoard().getCPUInterface().register_IO_In (0x12, this);
	}
}

RomManbow2::~RomManbow2()
{
	if (psg.get()) {
		getMotherBoard().getCPUInterface().unregister_IO_Out(0x10, this);
		getMotherBoard().getCPUInterface().unregister_IO_Out(0x11, this);
		getMotherBoard().getCPUInterface().unregister_IO_In (0x12, this);
	}
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

	if (psg.get()) {
		psgLatch = 0;
		psg->reset(time);
	}

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

byte RomManbow2::readIO(word port, EmuTime::param time)
{
	assert((port & 0xFF) == 0x12); (void)port;
	return psg->readRegister(psgLatch, time);
}

byte RomManbow2::peekIO(word port, EmuTime::param time) const
{
	assert((port & 0xFF) == 0x12); (void)port;
	return psg->peekRegister(psgLatch, time);
}

void RomManbow2::writeIO(word port, byte value, EmuTime::param time)
{
	if ((port & 0xFF) == 0x10) {
		psgLatch = value & 0x0F;
	} else {
		assert((port & 0xFF) == 0x11);
		psg->writeRegister(psgLatch, value, time);
	}
}


// version 1: initial version
// version 2: added optional built-in PSG
template<typename Archive>
void RomManbow2::serialize(Archive& ar, unsigned version)
{
	// skip MSXRom base class
	ar.template serializeBase<MSXDevice>(*this);

	ar.serialize("scc", *scc);
	if ((ar.versionAtLeast(version, 2)) && (psg.get())) {
		ar.serialize("psg", *psg);
		ar.serialize("psgLatch", psgLatch);
	}
	ar.serialize("flash", *flash);
	ar.serialize("bank", bank);
	ar.serialize("sccEnabled", sccEnabled);
}
INSTANTIATE_SERIALIZE_METHODS(RomManbow2);
REGISTER_MSXDEVICE(RomManbow2, "RomManbow2");

} // namespace openmsx
