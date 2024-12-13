#include "DalSoRiR2.hh"

#include "MSXCPUInterface.hh"
#include "serialize.hh"

#include "ranges.hh"

namespace openmsx {

// bit(s) of the memory bank selection registers regBank[n]
static constexpr byte REG_BANK_SEGMENT_MASK = (1 << 5) - 1; // 5-bits

// bit(s) of the frame selection registers regFrame[n]
static constexpr byte DISABLE = 1 << 7;
static constexpr byte REG_FRAME_SEGMENT_MASK = (1 << 3) - 1; // 3-bits

// bits of the configuration register regCfg
static constexpr byte ENA_S0 = 1 << 5;
static constexpr byte ENA_FW = 1 << 4;
static constexpr byte ENA_C4 = 1 << 1;
static constexpr byte ENA_C0 = 1 << 0;

DalSoRiR2::DalSoRiR2(const DeviceConfig& config)
	: MSXDevice(config)
	, ymf278b(getName(),
	          size_t(4096 * 1024), // 4MB RAM
	          config,
	          [this](bool mode0, std::span<const uint8_t> rom, std::span<const uint8_t> ram,
	                std::span<YMF278::Block128, 32> memPtrs) {
		     setupMemPtrs(mode0, rom, ram, memPtrs);
		  }, getCurrentTime())
	, sram(config, getName() + " RAM", "DalSoRi R2 RAM", 0x8000)
	, flash(getName() + " flash", AmdFlashChip::SST39SF010, {}, config, "flash")
	, dipSwitchBDIS(getCommandController(),
		getName() + " DIP switch BDIS",
		"Controls the BDIS DIP switch position. ON = Disable BIOS (flash memory access)", false)
	, dipSwitchMCFG(getCommandController(),
		getName() + " DIP switch MCFG",
		"Controls the MCFG DIP switch position. ON = Enable sample RAM instead of sample ROM on reset.", false)
	, dipSwitchIO_C0(getCommandController(),
		getName() + " DIP switch IO/C0",
		"Controls the IO/C0 DIP switch position. ON = Enable I/O addres C0H~C3H on reset. This is the MSX-AUDIO compatible I/O port range.", true)
	, dipSwitchIO_C4(getCommandController(),
		getName() + " DIP switch IO/C4",
		"Controls the IO/C4 DIP switch position. ON = Enable I/O addres C4H~C7H on reset. This is the MoonSound compatible I/O port range", true)
	, biosDisable(dipSwitchBDIS.getBoolean())
{
	dipSwitchBDIS.attach(*this);
	powerUp(getCurrentTime());
}

DalSoRiR2::~DalSoRiR2()
{
	setRegCfg(0); // unregister any FM I/O ports
	dipSwitchBDIS.detach(*this);
}

void DalSoRiR2::setupMemPtrs(
	bool mode0,
	std::span<const uint8_t> rom,
	std::span<const uint8_t> ram,
	std::span<YMF278::Block128, 32> memPtrs)
{
	// /MCS0: connected to either a 2MB ROM or a 2MB RAM chip (dynamically
	//        selectable via software)
	// /MCS1: connected to a 2MB RAM chip
	// /MCS2../MCS9: unused
	static constexpr auto k128 = YMF278::k128;

	// first 2MB, either ROM or RAM, both mode0 and mode1
	bool enableRam = regCfg & ENA_S0;
	auto mem0 = enableRam ? std::span<const uint8_t>{ram} : std::span<const uint8_t>{rom};
	for (auto i : xrange(16)) {
		memPtrs[i] = subspan<k128>(mem0, i * k128);
	}
	// second 2MB
	if (mode0) [[likely]] {
		// mode 0: RAM
		for (auto i : xrange(16, 32)) {
			memPtrs[i] = subspan<k128>(ram, i * k128);
		}
	} else {
		// mode 1: unmapped (normally shouldn't be used on DalSoRiR2)
		// Note: in this mode accessing region [0x00'0000, 0x10'0000) activates
		// both /MCS0 and /MCS1, thus two chips (ROM+RAM or RAM+RAM) get activated
		// simultaneously. That might cause problems on the real hardware. Here we
		// emulate it by ignoring the problem, as-if only /MCS0 was active.
		for (auto i : xrange(16, 32)) {
			memPtrs[i] = YMF278::nullBlock;
		}
	}
}

void DalSoRiR2::powerUp(EmuTime::param time)
{
	ymf278b.powerUp(time);
	reset(time);
}

void DalSoRiR2::reset(EmuTime::param time)
{
	ymf278b.reset(time);
	flash.reset();

	ranges::iota(regBank, 0);
	ranges::iota(regFrame, 0);

	setRegCfg((dipSwitchMCFG. getBoolean() ? ENA_S0 : 0) |
	          (dipSwitchIO_C0.getBoolean() ? ENA_C0 : 0) |
	          (dipSwitchIO_C4.getBoolean() ? ENA_C4 : 0));
}

void DalSoRiR2::setRegCfg(byte value)
{
	auto registerHelper = [this](bool enable, byte base) {
		if (enable) {
			getCPUInterface().  register_IO_InOut_range(base, 4, this);
		} else {
			getCPUInterface().unregister_IO_InOut_range(base, 4, this);
		}
	};
	if ((value ^ regCfg) & ENA_C0) { // enable C0 changed
		registerHelper((value & ENA_C0) != 0, 0xC0);
	}
	if ((value ^ regCfg) & ENA_C4) { // enable C4 changed
		registerHelper((value & ENA_C4) != 0, 0xC4);
	}

	if ((value ^ regCfg) & ENA_FW) {
		invalidateDeviceRWCache();
	}

	regCfg = value;

	ymf278b.setupMemoryPointers(); // ENA_S0 might have changed
}

byte DalSoRiR2::readIO(word port, EmuTime::param time)
{
	return ymf278b.readIO(port, time);
}

byte DalSoRiR2::peekIO(word port, EmuTime::param time) const
{
	return ymf278b.peekIO(port, time);
}

void DalSoRiR2::writeIO(word port, byte value, EmuTime::param time)
{
	ymf278b.writeIO(port, value, time);
}

byte* DalSoRiR2::getSramAddr(word addr)
{
	auto get = [&](byte frame) {
		return &sram[(addr & 0x0FFF) + (frame & REG_FRAME_SEGMENT_MASK) * 0x1000];
	};

	if ((0x3000 <= addr) && (addr < 0x4000)) {
		// SRAM frame 0
		return get(0);
	} else if ((0x7000 <= addr) && (addr < 0x8000) && ((regFrame[0] & DISABLE) == 0)) {
		// SRAM frame 1
		return get(regFrame[0]);
	} else if ((0xB000 <= addr) && (addr < 0xC000) && ((regFrame[1] & DISABLE) == 0)) {
		// SRAM frame 2
		return get(regFrame[1]);
	} else {
		return nullptr;
	}
}

unsigned DalSoRiR2::getFlashAddr(word addr) const
{
	auto page = addr >> 14;
	auto bank = regBank[page] & REG_BANK_SEGMENT_MASK;
	auto offset = addr & 0x3FFF;
	return 0x4000 * bank + offset;
}

byte DalSoRiR2::readMem(word addr, EmuTime::param /*time*/)
{
	if (const auto* r = getSramAddr(addr)) {
		return *r;
	} else if (!biosDisable) {
		return flash.read(getFlashAddr(addr));
	}
	return 0xFF;
}

byte DalSoRiR2::peekMem(word addr, EmuTime::param /*time*/) const
{
	if (const auto* r = const_cast<DalSoRiR2*>(this)->getSramAddr(addr)) {
		return *r;
	} else if (!biosDisable) {
		return flash.peek(getFlashAddr(addr));
	}
	return 0xFF;
}

const byte* DalSoRiR2::getReadCacheLine(word start) const
{
	if (const auto* r = const_cast<DalSoRiR2*>(this)->getSramAddr(start)) {
		return r;
	} else if (!biosDisable) {
		return flash.getReadCacheLine(getFlashAddr(start));
	}
	return unmappedRead.data();
}

void DalSoRiR2::writeMem(word addr, byte value, EmuTime::param /*time*/)
{
	if (auto* r = getSramAddr(addr)) {
		*r = value;
	} else if ((0x6000 <= addr) && (addr < 0x6400)) {
		auto bank = (addr >> 8) & 3;
		regBank[bank] = value;
		invalidateDeviceRWCache(bank * 0x4000, 0x4000);
	} else if ((0x6500 <= addr) && (addr < 0x6700)) {
		auto frame = (addr >> 9) & 1; // [0x6500,0x65FF] -> 0, [0x6600,0x66FF] -> 1
		regFrame[frame] = value;
		invalidateDeviceRWCache(0x7000 + frame * 0x4000, 0x1000);
	} else if ((0x6700 <= addr) && (addr < 0x6800)) {
		setRegCfg(value);
	} else if (!biosDisable && ((regCfg & ENA_FW) != 0)) {
		flash.write(getFlashAddr(addr), value);
	}
}

byte* DalSoRiR2::getWriteCacheLine(word start)
{
	if (auto* r = getSramAddr(start)) {
		return r;
	} else {
		// bank/frame/cfg registers or flash, not cacheable
		return nullptr;
	}
}

void DalSoRiR2::update(const Setting& setting) noexcept
{
	assert(&setting == &dipSwitchBDIS);
	auto newBiosDisable = dipSwitchBDIS.getBoolean();
	if (biosDisable != newBiosDisable) {
		biosDisable = newBiosDisable;
		invalidateDeviceRWCache();
	}
}

// version 1: initial version
template<typename Archive>
void DalSoRiR2::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	auto backupRegCfg = regCfg;
	ar.serialize("ymf278b",   ymf278b,
	             "sram",      sram,
	             "regBank",   regBank,
	             "regFrame",  regFrame,
	             "regCfg",    backupRegCfg);
	if constexpr (Archive::IS_LOADER) {
		setRegCfg(backupRegCfg); // register ports and setupMemoryPointers()
	}
	// no need to serialize biosDisable
}
INSTANTIATE_SERIALIZE_METHODS(DalSoRiR2);
REGISTER_MSXDEVICE(DalSoRiR2, "DalSoRiR2");

} // namespace openmsx
