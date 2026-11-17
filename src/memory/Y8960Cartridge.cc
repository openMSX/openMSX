// HRA!'s Y8960 sound cartridge.
//
// Mapper: four 8kB regions at 0x4000-0xBFFF, each with a bank register.
//   The hardware keeps the low 6 bits of a bank number:
//     0-15   ROM
//     16-31  RAM (read-only unless in RAM mode)
//     32-62  the same as 0-30
//     63     SCC registers in the upper 2kB of the region (0x1800-0x1FFF).
//            Only 0x9800-0x9FFF is where a Konami SCC has them.
//   0x4000-0x7FFF is mirrored at 0xC000-0xFFFF, 0x8000-0xBFFF at 0x0000-0x3FFF.
//
// Bank registers:
//   Normal mode: 0x5000-0x57FF, 0x7000-0x77FF, 0x9000-0x97FF, 0xB000-0xB7FF.
//   RAM mode:    0x48FC-0x48FF, mirrored every 0x100 up to 0x4FFF. RAM in
//                0x6000-0xBFFF is writable; 0x4000-0x5FFF holds these
//                registers and shows no SCC.
//   0x48FB (mirrored the same way) bit 0 selects RAM mode.
//
// SCC pan: 0x9900-0x9904, channel A-E, while 0x9800-0x9FFF shows the SCC.
//   Writes there don't reach the SCC; reads and the rest of 0x99xx do.
//
// Memory mapped registers, write-only, at 0x7FE0-0x7FFF while 0x6000-0x7FFF
// shows neither RAM nor the SCC:
//   0x7FF4/0x7FF5 OPLL address/data
//   0x7FF6 I/O Enabler1: bit 0 opens the OPLL ports
//   0x7FF7 SSG volume   0x7FF8 OPLL volume   0x7FF9 OPL2 volume
//   0x7FFA SCC volume   0x7FFD OPM volume    0x7FFE master volume
//
// I/O ports, all closed after reset like the FM-PAC's:
//   0x7C/0x7D OPLL address/data (write-only)

#include "Y8960Cartridge.hh"

#include "CacheLine.hh"
#include "MSXCPUInterface.hh"
#include "serialize.hh"

#include "outer.hh"
#include "xrange.hh"

#include <cmath>
#include <numbers>

namespace openmsx {

static constexpr unsigned ROM_BANKS = 16;
static constexpr unsigned RAM_BANKS = 16;
static constexpr uint8_t SCC_BANK = 0x3F;

// The registers of the digital mixer, which is also the layout of the
// "<name> mixer" debuggable:
//   0x00-0x04  volume of SSG, OPLL, OPL2, SCC, OPM
//   0x05       master volume
//   0x06-0x0E  OPLL pan, channel 1-9            (OPLL 50h-58h)
//   0x0F-0x18  OPL2 pan, channel 1-9 and ADPCM  (OPL2 F6h-FFh)
//   0x19-0x1D  SCC pan, channel A-E
//   0x1E-0x2D  SSG 20h-2Fh: volume (20h-22h, 24h-26h) and pan (28h-2Ah,
//              2Ch-2Eh) of channel A-C of both SSGs; 23h, 27h, 2Bh and 2Fh
//              are not connected and stay 0
static constexpr unsigned VOLUME   = 0x00;
static constexpr unsigned MASTER   = 0x05;
static constexpr unsigned OPLL_PAN = 0x06;
static constexpr unsigned OPL2_PAN = 0x0F;
static constexpr unsigned SCC_PAN  = 0x19;
static constexpr unsigned SSG_REGS = 0x1E;

[[nodiscard]] static constexpr bool isSsgConnected(unsigned reg)
{
	return (reg & 3) != 3;
}

// The NukeYKT core puts the rhythm out as HH, TOM, BD, SD, TC, after the 9
// melodic channels. Each takes the pan of the channel it sounds on: BD 7,
// HH and SD 8, TOM and TC 9.
static constexpr std::array<uint8_t, 5> OPLL_RHYTHM_CHANNEL = {7, 8, 6, 7, 8};

// The levels of the blocks (SSG, OPLL, OPL2, SCC, OPM) relative to each
// other, measured on the cartridge with all volume registers at their
// maximum. The config's <volume> is the level of the OPM, the loudest.
static constexpr std::array<float, 5> BLOCK_LEVEL = {
	15003.0f / 30000.0f, // SSG
	 7009.0f / 30000.0f, // OPLL
	 7433.0f / 30000.0f, // OPL2
	 7509.0f / 30000.0f, // SCC
	1.0f,                // OPM
};

struct PanGain { float left, right; };

// The pan registers: 1 is hard left, 128 the centre, 255 hard right, and 0
// is hard left like 1. The specification only gives those points; in between
// this is constant power, so the centre is 3dB down on both sides.
[[nodiscard]] static PanGain panGain(uint8_t pan)
{
	if (pan == 128) {
		// exactly symmetric, rather than cos() and sin() a rounding apart
		constexpr float c = std::numbers::sqrt2_v<float> / 2.0f;
		return {.left = c, .right = c};
	}
	if (pan == 0) pan = 1;
	float t = float(pan - 1) / 254.0f * (std::numbers::pi_v<float> / 2.0f);
	return {.left = std::cos(t), .right = std::sin(t)};
}

// The volume registers: 0 mutes, 1-127 is linear in steps of 1/128, and any
// value with bit 7 set is unity. 128 is the documented maximum.
[[nodiscard]] static float volumeGain(uint8_t volume)
{
	return (volume & 0x80) ? 1.0f : float(volume) / 128.0f;
}

Y8960Cartridge::Y8960Cartridge(const DeviceConfig& config, Rom&& rom_)
	: Rom8kBBlocks(config, std::move(rom_))
	, scc(getName() + " SCC", config, getCurrentTime(), SCC::Mode::Real)
	, opll(getName() + " OPLL", config, YM2413::Type::Y8960)
	, mixerDebuggable(getMotherBoard(), getName())
	, ram(config, getName() + " RAM", "Y8960 RAM", RAM_BANKS * 0x2000)
{
	scc.enableSetChannelFactors();
	opll.enableSetChannelFactors();
	powerUp(getCurrentTime());
}

Y8960Cartridge::~Y8960Cartridge()
{
	writeIoEnabler1(0);
}

void Y8960Cartridge::powerUp(EmuTime time)
{
	scc.powerUp(time);
	reset(time);
}

void Y8960Cartridge::reset(EmuTime time)
{
	writeIoEnabler1(0);
	ramEnabled = false;
	for (auto region : xrange(2u, 6u)) {
		setBank(region, uint8_t(region - 2));
	}

	scc.reset(time);
	opll.reset(time);
	opllLatch = 0;

	for (auto reg : xrange(VOLUME, MASTER + 1)) mixer[reg] = 0x80;
	for (auto reg : xrange(OPLL_PAN, SSG_REGS)) mixer[reg] = 128;
	for (auto r : xrange(8u)) {
		bool connected = isSsgConnected(r);
		mixer[SSG_REGS + r]     = connected ? 0x80 : 0; // volume
		mixer[SSG_REGS + 8 + r] = connected ? 128  : 0; // pan
	}
	applyAll(time);
}

bool Y8960Cartridge::isRamRegion(unsigned region) const
{
	auto bank = getBank(region) & 0x3F;
	return (bank != SCC_BANK) && (bank & 0x10);
}

bool Y8960Cartridge::isSccRegion(unsigned region) const
{
	if (ramEnabled && (region == 2)) return false;
	return (getBank(region) & 0x3F) == SCC_BANK;
}

bool Y8960Cartridge::isSccAddress(uint16_t address) const
{
	return (0x4000 <= address) && (address < 0xC000) &&
	       ((address & 0x1800) == 0x1800) && isSccRegion(address >> 13);
}

bool Y8960Cartridge::isMmioVisible() const
{
	// 0x7FE0-0x7FFF lies inside the SCC registers of the same region
	if (ramEnabled && isRamRegion(3)) return false;
	return !isSccRegion(3);
}

unsigned Y8960Cartridge::getRamAddress(uint16_t address) const
{
	return (getBank(address >> 13) & 0x0F) * 0x2000 + (address & 0x1FFF);
}

void Y8960Cartridge::setBank(unsigned region, uint8_t value)
{
	bankReg[region - 2] = value;
	auto block = value % ROM_BANKS;
	setRom(region, block);
	if (region < 4) {
		setRom(region + 4, block); // 0x4000-0x7FFF at 0xC000-0xFFFF
	} else {
		setRom(region - 4, block); // 0x8000-0xBFFF at 0x0000-0x3FFF
	}
	// After setRom(), which points the CPU's read cache at the ROM block:
	// the region may show RAM, or the SCC in its upper 2kB.
	invalidateDeviceRWCache(region << 13, 0x2000);
}

byte Y8960Cartridge::peekMem(uint16_t address, EmuTime time) const
{
	if (isSccAddress(address)) {
		return scc.peekMem(uint8_t(address), time);
	} else if (isRamRegion(address >> 13)) {
		return ram[getRamAddress(address)];
	} else {
		return Rom8kBBlocks::peekMem(address, time);
	}
}

byte Y8960Cartridge::readMem(uint16_t address, EmuTime time)
{
	if (isSccAddress(address)) {
		return scc.readMem(uint8_t(address), time);
	} else if (isRamRegion(address >> 13)) {
		return ram[getRamAddress(address)];
	} else {
		return Rom8kBBlocks::readMem(address, time);
	}
}

const byte* Y8960Cartridge::getReadCacheLine(uint16_t address) const
{
	if (isSccAddress(address)) {
		return nullptr;
	} else if (isRamRegion(address >> 13)) {
		return &ram[getRamAddress(address)];
	} else {
		return Rom8kBBlocks::getReadCacheLine(address);
	}
}

void Y8960Cartridge::writeMem(uint16_t address, byte value, EmuTime time)
{
	if ((address < 0x4800) || (address >= 0xC000)) return;

	if (isSccAddress(address)) {
		if ((0x9900 <= address) && (address <= 0x9904)) {
			writeMixer(SCC_PAN + address - 0x9900, value, time);
		} else {
			scc.writeMem(uint8_t(address), value, time);
		}
		return;
	}

	if (ramEnabled && (address >= 0x6000) && isRamRegion(address >> 13)) {
		ram[getRamAddress(address)] = value;
	}

	if (isMmioVisible()) {
		writeMmio(address, value, time);
	}

	if ((address & 0xF8FF) == 0x48FB) {
		bool newRamEnabled = value & 1;
		if (newRamEnabled != ramEnabled) {
			ramEnabled = newRamEnabled;
			invalidateDeviceRWCache(0x4000, 0x8000);
		}
	}

	if (!ramEnabled && ((address & 0x1800) == 0x1000)) {
		auto region = unsigned(address >> 13);
		if (value != getBank(region)) setBank(region, value);
	} else if (ramEnabled && ((address & 0xF8FC) == 0x48FC)) {
		auto region = unsigned(address & 3) + 2;
		if (value != getBank(region)) setBank(region, value);
	}
}

byte* Y8960Cartridge::getWriteCacheLine(uint16_t address)
{
	if ((address < 0x4800) || (address >= 0xC000)) {
		return unmappedWrite.data();
	} else if (address < 0x5000) {
		return nullptr; // RAM mode registers
	} else if ((address & CacheLine::HIGH) == 0x7F00) {
		return nullptr; // memory mapped registers
	} else if (isSccAddress(address)) {
		return nullptr;
	} else if ((address & 0x1800) == 0x1000) {
		return nullptr; // bank registers
	} else if (ramEnabled && (address >= 0x6000) && isRamRegion(address >> 13)) {
		return &ram[getRamAddress(address)];
	} else {
		return unmappedWrite.data();
	}
}

void Y8960Cartridge::writeMmio(uint16_t address, byte value, EmuTime time)
{
	using enum Block;
	switch (address) {
	case 0x7FF4: case 0x7FF5: writeOpll(address & 1, value, time); break;
	case 0x7FF6: writeIoEnabler1(value); break;
	case 0x7FF7: writeVolume(SSG,  value, time); break;
	case 0x7FF8: writeVolume(OPLL, value, time); break;
	case 0x7FF9: writeVolume(OPL2, value, time); break;
	case 0x7FFA: writeVolume(SCC,  value, time); break;
	case 0x7FFD: writeVolume(OPM,  value, time); break;
	case 0x7FFE: writeMixer(MASTER, value, time); break;
	default: break;
	}
}

void Y8960Cartridge::writeIoEnabler1(uint8_t value)
{
	value &= 0x01;
	if (value != ioEnabler1) {
		if (value) {
			getCPUInterface().register_IO_Out_range(0x7C, 2, this);
		} else {
			getCPUInterface().unregister_IO_Out_range(0x7C, 2, this);
		}
	}
	ioEnabler1 = value;
}

void Y8960Cartridge::writeIO(uint16_t port, byte value, EmuTime time)
{
	switch (port & 0xFF) {
	case 0x7C: case 0x7D: writeOpll(port & 1, value, time); break;
	default: break;
	}
}

void Y8960Cartridge::writeOpll(bool port, uint8_t value, EmuTime time)
{
	if (!port) {
		opllLatch = value;
	} else if ((0x50 <= opllLatch) && (opllLatch <= 0x58)) {
		writeMixer(OPLL_PAN + opllLatch - 0x50, value, time);
	}
	// the YM2413 sees the pan writes as well, and ignores them
	opll.writePort(port, value, time);
}

void Y8960Cartridge::writeVolume(Block block, uint8_t value, EmuTime time)
{
	writeMixer(VOLUME + unsigned(block), value, time);
}

void Y8960Cartridge::writeMixer(unsigned reg, uint8_t value, EmuTime time)
{
	if (reg >= SSG_REGS && !isSsgConnected(reg - SSG_REGS)) return;
	mixer[reg] = value;
	if (reg < MASTER) {
		applyVolume(Block(reg - VOLUME), time);
	} else if (reg == MASTER) {
		for (auto b : xrange(size_t(Block::NUM))) applyVolume(Block(b), time);
	} else {
		applyPan(reg, time);
	}
}

void Y8960Cartridge::applyVolume(Block block, EmuTime time)
{
	static_assert(BLOCK_LEVEL.size() == size_t(Block::NUM));
	float gain = BLOCK_LEVEL[size_t(block)]
	           * volumeGain(mixer[VOLUME + unsigned(block)]) * volumeGain(mixer[MASTER]);
	switch (block) {
	case Block::OPLL: opll.setSoftwareVolume(gain, time); break;
	case Block::SCC:  scc .setSoftwareVolume(gain, time); break;
	default: break;
	}
}

void Y8960Cartridge::applyPan(unsigned reg, EmuTime time)
{
	auto [left, right] = panGain(mixer[reg]);
	if ((OPLL_PAN <= reg) && (reg < OPLL_PAN + 9)) {
		auto ch = reg - OPLL_PAN;
		opll.setChannelFactors(ch, left, right, time);
		for (auto i : xrange(unsigned(OPLL_RHYTHM_CHANNEL.size()))) {
			if (OPLL_RHYTHM_CHANNEL[i] == ch) {
				opll.setChannelFactors(9 + i, left, right, time);
			}
		}
	} else if ((SCC_PAN <= reg) && (reg < SCC_PAN + 5)) {
		scc.setChannelFactors(reg - SCC_PAN, left, right, time);
	}
}

void Y8960Cartridge::applyAll(EmuTime time)
{
	for (auto b : xrange(size_t(Block::NUM))) applyVolume(Block(b), time);
	for (auto reg : xrange(OPLL_PAN, unsigned(mixer.size()))) applyPan(reg, time);
}

Y8960Cartridge::MixerDebuggable::MixerDebuggable(MSXMotherBoard& motherBoard_, const std::string& name_)
	: SimpleDebuggable(motherBoard_, name_ + " mixer", "Y8960 digital mixer", 0x2E)
{
}

uint8_t Y8960Cartridge::MixerDebuggable::read(unsigned address)
{
	const auto& cart = OUTER(Y8960Cartridge, mixerDebuggable);
	return cart.mixer[address];
}

void Y8960Cartridge::MixerDebuggable::write(unsigned address, uint8_t value, EmuTime time)
{
	auto& cart = OUTER(Y8960Cartridge, mixerDebuggable);
	cart.writeMixer(address, value, time);
}


template<typename Archive>
void Y8960Cartridge::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Rom8kBBlocks>(*this);
	ar.serialize("scc",           scc,
	             "opll",          opll,
	             "opllLatch",     opllLatch,
	             "mixer",         mixer,
	             "ram",           ram,
	             "bankReg",       bankReg,
	             "ramEnabled",    ramEnabled);
	if constexpr (Archive::IS_LOADER) {
		// the sound devices don't keep their software volume and channel
		// factors in a savestate
		applyAll(getCurrentTime());
	}
	// through the setter, which registers the ports that are open
	auto enabler1 = ioEnabler1;
	ar.serialize("ioEnabler1", enabler1);
	if constexpr (Archive::IS_LOADER) writeIoEnabler1(enabler1);
}
INSTANTIATE_SERIALIZE_METHODS(Y8960Cartridge);
REGISTER_MSXDEVICE(Y8960Cartridge, "Y8960Cartridge");

} // namespace openmsx
