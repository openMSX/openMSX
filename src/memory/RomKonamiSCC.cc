// KONAMI 8kB cartridges with SCC
//
// this type is used by Konami cartridges that do have an SCC and some others
// examples of cartridges: Nemesis 2, Nemesis 3, King's Valley 2, Space Manbow
// Solid Snake, Quarth, Ashguine 1, Animal, Arkanoid 2, ...
// Those last 3 were probably modified ROM images, they should be ASCII8
//
// The address to change banks:
//  bank 1: 0x5000 - 0x57ff (0x5000 used)
//  bank 2: 0x7000 - 0x77ff (0x7000 used)
//  bank 3: 0x9000 - 0x97ff (0x9000 used)
//  bank 4: 0xB000 - 0xB7ff (0xB000 used)

#include "RomKonamiSCC.hh"
#include "CacheLine.hh"
#include "MSXMotherBoard.hh"
#include "MSXCliComm.hh"
#include "sha1.hh"
#include "serialize.hh"
#include "xrange.hh"

namespace openmsx {

// minimal attempt to avoid seeing this warning too often
static Sha1Sum alreadyWarnedForSha1Sum;

RomKonamiSCC::RomKonamiSCC(const DeviceConfig& config, Rom&& rom_)
	: Rom8kBBlocks(config, std::move(rom_))
	, scc("SCC", config, getCurrentTime())
{
	// warn if a ROM is used that would not work on a real KonamiSCC mapper
	if ((rom.size() > 512 * 1024) && alreadyWarnedForSha1Sum != rom.getOriginalSHA1()) {
		getMotherBoard().getMSXCliComm().printWarning(
			"The size of this ROM image is larger than 512kB, "
			"which is not supported on real Konami SCC mapper "
			"chips!");
		alreadyWarnedForSha1Sum = rom.getOriginalSHA1();
	}
	powerUp(getCurrentTime());
}

void RomKonamiSCC::powerUp(EmuTime::param time)
{
	scc.powerUp(time);
	reset(time);
}

void RomKonamiSCC::bankSwitch(unsigned page, unsigned block)
{
	setRom(page, block);

	// Note: the mirror behavior is different from RomKonami !
	if (page == 2 || page == 3) {
		// [0x4000-0x8000), mirrored in [0xC000-0x10000)
		setRom(page + 4, block);
	} else if (page == 4 || page == 5) {
		// [0x8000-0xC000), mirrored in [0x0000-0x4000)
		setRom(page - 4, block);
	} else {
		assert(false);
	}
}

void RomKonamiSCC::reset(EmuTime::param time)
{
	for (auto i : xrange(2, 6)) {
		bankSwitch(i, i - 2);
	}

	sccEnabled = false;
	scc.reset(time);
}

byte RomKonamiSCC::peekMem(word address, EmuTime::param time) const
{
	if (sccEnabled && (0x9800 <= address) && (address < 0xA000)) {
		return scc.peekMem(narrow_cast<uint8_t>(address & 0xFF), time);
	} else {
		return Rom8kBBlocks::peekMem(address, time);
	}
}

byte RomKonamiSCC::readMem(word address, EmuTime::param time)
{
	if (sccEnabled && (0x9800 <= address) && (address < 0xA000)) {
		return scc.readMem(narrow_cast<uint8_t>(address & 0xFF), time);
	} else {
		return Rom8kBBlocks::readMem(address, time);
	}
}

const byte* RomKonamiSCC::getReadCacheLine(word address) const
{
	if (sccEnabled && (0x9800 <= address) && (address < 0xA000)) {
		// don't cache SCC
		return nullptr;
	} else {
		return Rom8kBBlocks::getReadCacheLine(address);
	}
}

void RomKonamiSCC::writeMem(word address, byte value, EmuTime::param time)
{
	if ((address < 0x5000) || (address >= 0xC000)) {
		return;
	}
	if (sccEnabled && (0x9800 <= address) && (address < 0xA000)) {
		// write to SCC
		scc.writeMem(narrow_cast<uint8_t>(address & 0xFF), value, time);
		return;
	}
	if ((address & 0xF800) == 0x9000) {
		// SCC enable/disable
		bool newSccEnabled = ((value & 0x3F) == 0x3F);
		if (newSccEnabled != sccEnabled) {
			sccEnabled = newSccEnabled;
			invalidateDeviceRWCache(0x9800, 0x0800);
		}
	}
	if ((address & 0x1800) == 0x1000) {
		// page selection
		auto region = address >> 13;
		bankSwitch(region, value);
		if ((region == 4) && sccEnabled) {
			invalidateDeviceRCache(0x9800, 0x0800);
		}
	}
}

byte* RomKonamiSCC::getWriteCacheLine(word address) const
{
	if ((address < 0x5000) || (address >= 0xC000)) {
		return unmappedWrite.data();
	} else if (sccEnabled && (0x9800 <= address) && (address < 0xA000)) {
		// write to SCC
		return nullptr;
	} else if ((address & 0xF800) == (0x9000 & CacheLine::HIGH)) {
		// SCC enable/disable
		return nullptr;
	} else if ((address & 0x1800) == (0x1000 & CacheLine::HIGH)) {
		// page selection
		return nullptr;
	} else {
		return unmappedWrite.data();
	}
}

template<typename Archive>
void RomKonamiSCC::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Rom8kBBlocks>(*this);
	ar.serialize("scc",        scc,
	             "sccEnabled", sccEnabled);
}
INSTANTIATE_SERIALIZE_METHODS(RomKonamiSCC);
REGISTER_MSXDEVICE(RomKonamiSCC, "RomKonamiSCC");

} // namespace openmsx
