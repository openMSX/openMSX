#include "RomManbow2.hh"
#include "AY8910.hh"
#include "DummyAY8910Periphery.hh"
#include "SCC.hh"
#include "MSXCPUInterface.hh"
#include "narrow.hh"
#include "one_of.hh"
#include "ranges.hh"
#include "serialize.hh"
#include "unreachable.hh"
#include "xrange.hh"
#include <array>
#include <cassert>
#include <memory>

namespace openmsx {

using Info = AmdFlash::SectorInfo;

// 512kB, only last 64kB writable
static constexpr auto config1 = [] {
	std::array<Info, 512 / 64> result = {};
	ranges::fill(result, Info{64 * 1024, true}); // read-only
	result[7].writeProtected = false;
	return result;
}();
// 512kB, only 128kB writable
static constexpr auto config2 = [] {
	std::array<Info, 512 / 64> result = {};
	ranges::fill(result, Info{64 * 1024, true}); // read-only
	result[4].writeProtected = false;
	result[5].writeProtected = false;
	return result;
}();
// fully writeable, 512kB
static constexpr auto config3 = [] {
	std::array<Info, 512 / 64> result = {};
	ranges::fill(result, Info{64 * 1024, false});
	return result;
}();
// fully writeable, 2MB
static constexpr auto config4 = [] {
	std::array<Info, 2048 / 64> result = {};
	ranges::fill(result, Info{64 * 1024, false});
	return result;
}();
[[nodiscard]] static constexpr std::span<const Info> getSectorInfo(RomType type)
{
	switch (type) {
	case ROM_MANBOW2:
	case ROM_MANBOW2_2:
		return config1;
	case ROM_HAMARAJANIGHT:
		return config2;
	case ROM_MEGAFLASHROMSCC:
		return config3;
	case ROM_RBSC_FLASH_KONAMI_SCC:
		return config4;
	default:
		UNREACHABLE;
		return config1; // dummy
	}
}

RomManbow2::RomManbow2(const DeviceConfig& config, Rom&& rom_,
                       RomType type)
	: MSXRom(config, std::move(rom_))
	, scc((type != ROM_RBSC_FLASH_KONAMI_SCC)
		? std::make_unique<SCC>(
			getName() + " SCC", config, getCurrentTime())
		: nullptr)
	, psg((type == one_of(ROM_MANBOW2_2, ROM_HAMARAJANIGHT))
		? std::make_unique<AY8910>(
			getName() + " PSG", DummyAY8910Periphery::instance(),
			config, getCurrentTime())
		: nullptr)
	, flash(rom, getSectorInfo(type),
	        type == ROM_RBSC_FLASH_KONAMI_SCC ? 0x01AD : 0x01A4,
		AmdFlash::Addressing::BITS_11, config)
	, romBlockDebug(*this, bank, 0x4000, 0x8000, 13)
{
	powerUp(getCurrentTime());

	if (psg) {
		getCPUInterface().register_IO_Out(0x10, this);
		getCPUInterface().register_IO_Out(0x11, this);
		getCPUInterface().register_IO_In (0x12, this);
	}
}

RomManbow2::~RomManbow2()
{
	if (psg) {
		getCPUInterface().unregister_IO_Out(0x10, this);
		getCPUInterface().unregister_IO_Out(0x11, this);
		getCPUInterface().unregister_IO_In (0x12, this);
	}
}

void RomManbow2::powerUp(EmuTime::param time)
{
	if (scc) {
		scc->powerUp(time);
	}
	reset(time);
}

void RomManbow2::reset(EmuTime::param time)
{
	for (auto i : xrange(4)) {
		setRom(i, byte(i));
	}

	sccEnabled = false;
	if (scc) {
		scc->reset(time);
	}

	if (psg) {
		psgLatch = 0;
		psg->reset(time);
	}

	flash.reset();
}

void RomManbow2::setRom(unsigned region, byte block)
{
	assert(region < 4);
	auto nrBlocks = narrow<unsigned>(flash.size() / 0x2000);
	bank[region] = block & narrow<byte>(nrBlocks - 1);
	invalidateDeviceRCache(0x4000 + region * 0x2000, 0x2000);
}

byte RomManbow2::peekMem(word address, EmuTime::param time) const
{
	if (sccEnabled && (0x9800 <= address) && (address < 0xA000)) {
		return scc->peekMem(narrow_cast<uint8_t>(address & 0xFF), time);
	} else if ((0x4000 <= address) && (address < 0xC000)) {
		unsigned page = (address - 0x4000) / 0x2000;
		unsigned addr = (address & 0x1FFF) + 0x2000 * bank[page];
		return flash.peek(addr);
	} else {
		return 0xFF;
	}
}

byte RomManbow2::readMem(word address, EmuTime::param time)
{
	if (sccEnabled && (0x9800 <= address) && (address < 0xA000)) {
		return scc->readMem(narrow_cast<uint8_t>(address & 0xFF), time);
	} else if ((0x4000 <= address) && (address < 0xC000)) {
		unsigned page = (address - 0x4000) / 0x2000;
		unsigned addr = (address & 0x1FFF) + 0x2000 * bank[page];
		return flash.read(addr);
	} else {
		return 0xFF;
	}
}

const byte* RomManbow2::getReadCacheLine(word address) const
{
	if (sccEnabled && (0x9800 <= address) && (address < 0xA000)) {
		return nullptr;
	} else if ((0x4000 <= address) && (address < 0xC000)) {
		unsigned page = (address - 0x4000) / 0x2000;
		unsigned addr = (address & 0x1FFF) + 0x2000 * bank[page];
		return flash.getReadCacheLine(addr);
	} else {
		return unmappedRead.data();
	}
}

void RomManbow2::writeMem(word address, byte value, EmuTime::param time)
{
	if (sccEnabled && (0x9800 <= address) && (address < 0xA000)) {
		// write to SCC
		scc->writeMem(narrow_cast<uint8_t>(address & 0xff), value, time);
		// note: writes to SCC also go to flash
		//    thanks to 'enen' for testing this
	}
	if ((0x4000 <= address) && (address < 0xC000)) {
		unsigned page = (address - 0x4000) / 0x2000;
		unsigned addr = (address & 0x1FFF) + 0x2000 * bank[page];
		flash.write(addr, value);

		if (scc && ((address & 0xF800) == 0x9000)) {
			// SCC enable/disable
			sccEnabled = ((value & 0x3F) == 0x3F);
			invalidateDeviceRCache(0x9800, 0x0800);
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
		return nullptr;
	} else {
		return unmappedWrite.data();
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
// version 3: made SCC optional (for ROM_RBSC_FLASH_KONAMI_SCC)
template<typename Archive>
void RomManbow2::serialize(Archive& ar, unsigned version)
{
	// skip MSXRom base class
	ar.template serializeBase<MSXDevice>(*this);

	if (scc) {
		ar.serialize("scc",        *scc,
		             "sccEnabled", sccEnabled);
	}
	if ((ar.versionAtLeast(version, 2)) && psg) {
		ar.serialize("psg",      *psg,
		             "psgLatch", psgLatch);
	}
	ar.serialize("flash", flash,
	             "bank",  bank);
}
INSTANTIATE_SERIALIZE_METHODS(RomManbow2);
REGISTER_MSXDEVICE(RomManbow2, "RomManbow2");

} // namespace openmsx
