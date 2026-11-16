// HRA!'s Y8960 sound cartridge.
//
// Mapper: four 8kB regions at 0x4000-0xBFFF, each with a bank register.
//   The hardware keeps the low 6 bits of a bank number:
//     0-15   ROM
//     16-31  RAM (read-only unless in RAM mode)
//     32-62  the same as 0-30
//     63     SCC registers in the upper 2kB of the region (0x1800-0x1FFF).
//            Only 0x9800-0x9FFF is where a Konami SCC has them.
//   0x4000-0x7FFF is mirrored at 0xC000-0xFFFF, 0x8000-0xBFFF at 0x0000-0x3FFF,
//   for reading only and without the SCC.
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
// Memory mapped registers, write-only but for 0x7FFF, at 0x7FE0-0x7FFF while
// 0x6000-0x7FFF shows neither RAM nor the SCC:
//   0x7FF0/0x7FF1 SSG address/data
//   0x7FF2/0x7FF3 OPL2 address/data
//   0x7FF4/0x7FF5 OPLL address/data
//   0x7FF6 I/O Enabler1: bit 0 opens the OPLL ports
//   0x7FF7 SSG volume   0x7FF8 OPLL volume   0x7FF9 OPL2 volume
//   0x7FFA SCC volume   0x7FFD OPM volume    0x7FFE master volume
//   0x7FFB/0x7FFC OPM address/data, in between the volumes
//   0x7FFF I/O Enabler2, readable: bit 0 opens the OPL2 ports, bit 1 the
//          SSG's, bit 2 the OPM's, bit 7 the MSX-TIMER's
//
// I/O ports, all closed after reset like the FM-PAC's:
//   0x7C/0x7D OPLL address/data (write-only)
//   0x7E/0x7F OPM address and data/status (the status on 0x7F only)
//   0xA0/0xA1 SSG address/data (write-only), shared with the MSX's own PSG
//   0xB0-0xB3 MSX-TIMER. Its counters and interrupt run on while the ports
//             are closed: the enable bit only gates the address decoding.
//   0x40-0x4F System Controller (switched I/O id 0x80), always there
//   0xC0/0xC1 OPL2 address and status/data

#include "Y8960Cartridge.hh"

#include "CacheLine.hh"
#include "DummyAY8910Periphery.hh"
#include "MSXCPUInterface.hh"
#include "MSXMotherBoard.hh"
#include "serialize.hh"

#include "outer.hh"
#include "xrange.hh"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace openmsx {

static constexpr unsigned ROM_BANKS = 16;
static constexpr unsigned RAM_BANKS = 16;
static constexpr uint8_t SCC_BANK = 0x3F;

static constexpr uint8_t SYSTEM_CONTROLLER_ID = 0x80; // 40h
static constexpr uint8_t BOARD_ID = 0x02;             // 41h: this cartridge
static constexpr uint8_t FUNCTION_CONFIG_ROM = 0x01;  // 42h

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

// Y8950 puts the rhythm out as BD, SD, CYM, HH, TOM, then the ADPCM. The
// rhythm takes the pans of channels 7-9 like the OPLL's, the ADPCM its own.
static constexpr std::array<uint8_t, 6> OPL2_RHYTHM_CHANNEL = {6, 7, 8, 7, 8, 9};

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

// The clock of the two SSGs (the default of AY8910).
static constexpr float SSG_CLOCK = 3579545.0f / 2.0f;

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
	, MSXSwitchedDevice(getMotherBoard(), SYSTEM_CONTROLLER_ID)
	// These devices have stereo output: the digital mixer of the Y8960 has
	// a pan (and for the SSGs also a volume) register per channel, see
	// applyPan().
	, scc(getName() + " SCC", config, getCurrentTime(), SCC::Mode::Real, SoundDevice::Output::STEREO)
	, opll(getName() + " OPLL", config, YM2413::Type::Y8960, SoundDevice::Output::STEREO)
	, opl2(getName() + " OPL2", config, 256 * 1024, getCurrentTime(), noPeriphery, Y8950::Type::Y8960,
	       SoundDevice::Output::STEREO)
	, ssg{AY8910(getName() + " SSG 0", DummyAY8910Periphery::instance(), config, getCurrentTime(), AY8910::Type::YM2149,
	             SSG_CLOCK, SoundDevice::Output::STEREO),
	      AY8910(getName() + " SSG 1", DummyAY8910Periphery::instance(), config, getCurrentTime(), AY8910::Type::YM2149,
	             SSG_CLOCK, SoundDevice::Output::STEREO)}
	, opm(getName() + " OPM", "Y8960 OPM", config, getCurrentTime(), YM2151::Variant::YM2151)
	, timer(getMotherBoard(), getName() + " MSX-TIMER")
	, mixerDebuggable(getMotherBoard(), getName())
	, ram(config, getName() + " RAM", "Y8960 RAM", RAM_BANKS * 0x2000)
{
	powerUp(getCurrentTime());
}

Y8960Cartridge::~Y8960Cartridge()
{
	writeIoEnabler1(0);
	writeIoEnabler2(0);
}

void Y8960Cartridge::powerUp(EmuTime time)
{
	scc.powerUp(time);
	opl2.clearRam();
	reset(time);
}

void Y8960Cartridge::reset(EmuTime time)
{
	writeIoEnabler1(0);
	writeIoEnabler2(0);
	ramEnabled = false;
	for (auto region : xrange(2u, 6u)) {
		setBank(region, uint8_t(region - 2));
	}

	scc.reset(time);
	opll.reset(time);
	opllLatch = 0;
	opl2.reset(time);
	opl2Latch = 0;
	for (auto& s : ssg) s.reset(time);
	ssgLatch = 0;
	opm.reset(time);
	opmLatch = 0;
	timer.reset(time);

	for (auto reg : xrange(VOLUME, MASTER + 1)) mixer[reg] = 0x80;
	for (auto reg : xrange(OPLL_PAN, SSG_REGS)) mixer[reg] = 128;
	for (auto r : xrange(8u)) {
		bool connected = isSsgConnected(r);
		mixer[SSG_REGS + r]     = connected ? 0x80 : 0; // volume
		mixer[SSG_REGS + 8 + r] = connected ? 128  : 0; // pan
	}
	applyAll(time);

	boardSelected = false;
	romFunction = false;
}

// The region whose bank an address shows. Only a cartridge inserted with
// -romtype Y8960Cartridge sits at 0x0000-0xFFFF and has the mirrors; the
// extension places it at 0x4000-0xBFFF.
[[nodiscard]] static unsigned bankRegion(uint16_t address)
{
	unsigned region = address >> 13;
	return ((region < 2) || (region >= 6)) ? (region ^ 4) : region;
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

bool Y8960Cartridge::isIoEnabler2(uint16_t address) const
{
	return (address == 0x7FFF) && isMmioVisible();
}

unsigned Y8960Cartridge::getRamAddress(uint16_t address) const
{
	return (getBank(bankRegion(address)) & 0x0F) * 0x2000 + (address & 0x1FFF);
}

void Y8960Cartridge::setBank(unsigned region, uint8_t value)
{
	bankReg[region - 2] = value;
	auto block = value % ROM_BANKS;
	unsigned mirror = region ^ 4;
	setRom(region, block);
	setRom(mirror, block);
	if (isRamRegion(region)) {
		// The RAM isn't in the ROM image, so setRom() gave 'romblocks'
		// the ROM block. Number RAM bank n as 16 + n instead, as it is
		// selected in 0-31. The SCC bank keeps 15: below the registers
		// it shows ROM block 15.
		blockNr[region] = blockNr[mirror] = uint8_t(ROM_BANKS + (value % RAM_BANKS));
	}
	// After setRom(), which points the CPU's read cache at the ROM block:
	// both may show RAM, and the region the SCC in its upper 2kB.
	invalidateDeviceRWCache(region << 13, 0x2000);
	invalidateDeviceRWCache(mirror << 13, 0x2000);
}

byte Y8960Cartridge::peekMem(uint16_t address, EmuTime time) const
{
	if (isIoEnabler2(address)) {
		return ioEnabler2;
	} else if (isSccAddress(address)) {
		return scc.peekMem(uint8_t(address), time);
	} else if (isRamRegion(bankRegion(address))) {
		return ram[getRamAddress(address)];
	} else {
		return Rom8kBBlocks::peekMem(address, time);
	}
}

byte Y8960Cartridge::readMem(uint16_t address, EmuTime time)
{
	if (isIoEnabler2(address)) {
		return ioEnabler2;
	} else if (isSccAddress(address)) {
		return scc.readMem(uint8_t(address), time);
	} else if (isRamRegion(bankRegion(address))) {
		return ram[getRamAddress(address)];
	} else {
		return Rom8kBBlocks::readMem(address, time);
	}
}

const byte* Y8960Cartridge::getReadCacheLine(uint16_t address) const
{
	if (isIoEnabler2(address | CacheLine::LOW)) {
		return nullptr;
	} else if (isSccAddress(address)) {
		return nullptr;
	} else if (isRamRegion(bankRegion(address))) {
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
	case 0x7FF0: case 0x7FF1: writeSsg (address & 1, value, time); break;
	case 0x7FF2: case 0x7FF3: writeOpl2(address & 1, value, time); break;
	case 0x7FF4: case 0x7FF5: writeOpll(address & 1, value, time); break;
	case 0x7FF6: writeIoEnabler1(value); break;
	case 0x7FF7: writeVolume(SSG,  value, time); break;
	case 0x7FF8: writeVolume(OPLL, value, time); break;
	case 0x7FF9: writeVolume(OPL2, value, time); break;
	case 0x7FFA: writeVolume(SCC,  value, time); break;
	case 0x7FFB: writeOpm(false, value, time); break;
	case 0x7FFC: writeOpm(true,  value, time); break;
	case 0x7FFD: writeVolume(OPM,  value, time); break;
	case 0x7FFE: writeMixer(MASTER, value, time); break;
	case 0x7FFF: writeIoEnabler2(value); break;
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

void Y8960Cartridge::writeIoEnabler2(uint8_t value)
{
	value &= 0x87;
	auto changed = value ^ ioEnabler2;
	auto& cpu = getCPUInterface();
	if (changed & 0x01) {
		if (value & 0x01) {
			cpu.register_IO_InOut_range(0xC0, 2, this);
		} else {
			cpu.unregister_IO_InOut_range(0xC0, 2, this);
		}
	}
	if (changed & 0x02) {
		if (value & 0x02) {
			cpu.register_IO_Out_range(0xA0, 2, this);
		} else {
			cpu.unregister_IO_Out_range(0xA0, 2, this);
		}
	}
	if (changed & 0x04) {
		if (value & 0x04) {
			cpu.register_IO_InOut_range(0x7E, 2, this);
		} else {
			cpu.unregister_IO_InOut_range(0x7E, 2, this);
		}
	}
	if (changed & 0x80) {
		if (value & 0x80) {
			cpu.register_IO_InOut_range(0xB0, 4, this);
		} else {
			cpu.unregister_IO_InOut_range(0xB0, 4, this);
		}
	}
	ioEnabler2 = value;
}

byte Y8960Cartridge::readIO(uint16_t port, EmuTime time)
{
	switch (port & 0xFF) {
	case 0x7E: case 0x7F: return readOpm(port & 1);
	case 0xB0: case 0xB1: case 0xB2: case 0xB3: return timer.readPort(port & 3, time);
	case 0xC0: case 0xC1: return readOpl2(port & 1, time);
	default: return 0xFF;
	}
}

byte Y8960Cartridge::peekIO(uint16_t port, EmuTime time) const
{
	switch (port & 0xFF) {
	case 0x7E: case 0x7F: return readOpm(port & 1);
	case 0xB0: case 0xB1: case 0xB2: case 0xB3: return timer.peekPort(port & 3, time);
	case 0xC0: case 0xC1: return peekOpl2(port & 1, time);
	default: return 0xFF;
	}
}

void Y8960Cartridge::writeIO(uint16_t port, byte value, EmuTime time)
{
	switch (port & 0xFF) {
	case 0x7C: case 0x7D: writeOpll(port & 1, value, time); break;
	case 0x7E: case 0x7F: writeOpm (port & 1, value, time); break;
	case 0xA0: case 0xA1: writeSsg (port & 1, value, time); break;
	case 0xB0: case 0xB1: case 0xB2: case 0xB3: timer.writePort(port & 3, value, time); break;
	case 0xC0: case 0xC1: writeOpl2(port & 1, value, time); break;
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

// Unlike a Y8950, the data port reads back only the ADPCM data (0Fh).
uint8_t Y8960Cartridge::readOpl2(bool port, EmuTime time)
{
	if (!port) return opl2.readStatus(time);
	return (opl2Latch == 0x0F) ? opl2.readReg(opl2Latch, time) : 0xFF;
}

uint8_t Y8960Cartridge::peekOpl2(bool port, EmuTime time) const
{
	if (!port) return opl2.peekStatus(time);
	return (opl2Latch == 0x0F) ? opl2.peekReg(opl2Latch, time) : 0xFF;
}

void Y8960Cartridge::writeOpl2(bool port, uint8_t value, EmuTime time)
{
	if (!port) {
		opl2Latch = value;
		return;
	}
	if (opl2Latch >= 0xF6) {
		writeMixer(OPL2_PAN + opl2Latch - 0xF6, value, time);
	}
	// the Y8950 sees the pan writes as well, and ignores them
	opl2.writeReg(opl2Latch, value, time);
}

void Y8960Cartridge::writeSsg(bool port, uint8_t value, EmuTime time)
{
	if (!port) {
		ssgLatch = value & 0x3F; // 40h-FFh are 00h-3Fh again
	} else if (ssgLatch < 0x20) {
		ssg[ssgLatch >> 4].writeRegister(ssgLatch & 0x0F, value, time);
	} else if (ssgLatch < 0x30) {
		writeMixer(SSG_REGS + ssgLatch - 0x20, value, time);
	}
}

// The status is on the data port only. Its busy bit stays 0: a write takes
// effect at once here, where the cartridge queues writes and shows the queue
// as busy.
uint8_t Y8960Cartridge::readOpm(bool port) const
{
	return port ? opm.readStatus() : 0xFF;
}

void Y8960Cartridge::writeOpm(bool port, uint8_t value, EmuTime time)
{
	if (!port) {
		opmLatch = value;
	} else {
		opm.writeReg(opmLatch, value, time);
	}
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
	case Block::SSG:  for (auto& s : ssg) s.setSoftwareVolume(gain, time); break;
	case Block::OPLL: opll.setSoftwareVolume(gain, time); break;
	case Block::OPL2: opl2.setSoftwareVolume(gain, time); break;
	case Block::SCC:  scc .setSoftwareVolume(gain, time); break;
	case Block::OPM:  opm .setSoftwareVolume(gain, time); break;
	default: break;
	}
}

void Y8960Cartridge::applyPan(unsigned reg, EmuTime time)
{
	if (reg >= SSG_REGS) {
		// 20h-27h volume, 28h-2Fh pan; bit 2 the SSG, bits 1-0 its channel
		auto r = (reg - SSG_REGS) & 7;
		if (!isSsgConnected(r)) return;
		auto pan = panGain(mixer[SSG_REGS + 8 + r]);
		float vol = volumeGain(mixer[SSG_REGS + r]);
		ssg[r >> 2].setChannelFactors(r & 3, pan.left * vol, pan.right * vol, time);
		return;
	}
	auto [left, right] = panGain(mixer[reg]);
	// A pan register of the OPLL or OPL2 is for a melody channel and for
	// the rhythm sounds that are played on that channel: set them all at
	// once. 'ch' is the melody channel (if any), the rhythm sounds follow
	// the 9 melody channels.
	auto setWithRhythm = [&](SoundDevice& device, unsigned ch, bool melody,
	                         std::span<const uint8_t> rhythmChannel) {
		std::array<SoundDevice::ChannelFactors, 1 + 6> factors;
		size_t num = 0;
		if (melody) factors[num++] = {.channel = ch, .left = left, .right = right};
		for (auto i : xrange(unsigned(rhythmChannel.size()))) {
			if (rhythmChannel[i] == ch) {
				factors[num++] = {.channel = 9 + i, .left = left, .right = right};
			}
		}
		device.setChannelFactors(std::span{factors}.first(num), time);
	};
	if ((OPLL_PAN <= reg) && (reg < OPLL_PAN + 9)) {
		setWithRhythm(opll, reg - OPLL_PAN, true, OPLL_RHYTHM_CHANNEL);
	} else if ((OPL2_PAN <= reg) && (reg < OPL2_PAN + 10)) {
		auto ch = reg - OPL2_PAN;
		setWithRhythm(opl2, ch, ch < 9, OPL2_RHYTHM_CHANNEL);
	} else if ((SCC_PAN <= reg) && (reg < SCC_PAN + 5)) {
		scc.setChannelFactors(reg - SCC_PAN, left, right, time);
	}
}

void Y8960Cartridge::applyAll(EmuTime time)
{
	for (auto b : xrange(size_t(Block::NUM))) applyVolume(Block(b), time);
	for (auto reg : xrange(OPLL_PAN, unsigned(mixer.size()))) applyPan(reg, time);
}

void Y8960Cartridge::NoPeriphery::write(uint4_t /*outputs*/, uint4_t /*values*/, EmuTime /*time*/)
{
	// nothing is connected to the OPL2's general purpose I/O
}

uint4_t Y8960Cartridge::NoPeriphery::read(EmuTime /*time*/)
{
	return 15;
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

// The System Controller: 41h selects a board by its id (02h is this
// cartridge) and 42h a function (01h is the Config ROM reader/writer); each
// reads back the complement of what selected it. Only this identification is
// emulated: the Config ROM reader/writer behind 43h-47h reads FFh and takes
// no writes. The hardware forgets the selection when 40h is written with
// anything but 80h. The device switch doesn't tell a switched device that it
// was deselected, so here the selection lasts until 41h is written again or a
// reset.
uint8_t Y8960Cartridge::readSwitchedIO(uint16_t port, EmuTime time)
{
	return peekSwitchedIO(port, time);
}

uint8_t Y8960Cartridge::peekSwitchedIO(uint16_t port, EmuTime /*time*/) const
{
	switch (port & 0x0F) {
	case 0x00: return uint8_t(~SYSTEM_CONTROLLER_ID);
	case 0x01: return boardSelected ? uint8_t(~BOARD_ID) : 0xFF;
	case 0x02: return romFunction ? uint8_t(~FUNCTION_CONFIG_ROM) : 0xFF;
	default:   return 0xFF;
	}
}

void Y8960Cartridge::writeSwitchedIO(uint16_t port, uint8_t value, EmuTime /*time*/)
{
	switch (port & 0x0F) {
	case 0x01:
		boardSelected = value == BOARD_ID;
		if (!boardSelected) romFunction = false;
		break;
	case 0x02:
		if (boardSelected) romFunction = value == FUNCTION_CONFIG_ROM;
		break;
	default:
		break;
	}
}


Y8960Cartridge::Timer::Timer(MSXMotherBoard& motherBoard, const std::string& name)
	: counters{Counter(*this, motherBoard.getScheduler()), Counter(*this, motherBoard.getScheduler()),
	           Counter(*this, motherBoard.getScheduler()), Counter(*this, motherBoard.getScheduler())}
	, irq(motherBoard, name + ".IRQ")
{
}

void Y8960Cartridge::Timer::reset(EmuTime time)
{
	for (auto& c : counters) c.reset(time);
	index = 0;
	select = 0;
	irq.reset();
}

uint8_t Y8960Cartridge::Timer::readPort(unsigned port, EmuTime time)
{
	switch (port & 3) {
	case 1: case 2: case 3:
		for (auto& c : counters) c.sync(time);
		updateIrq();
		break;
	}
	return peekPort(port, time);
}

uint8_t Y8960Cartridge::Timer::peekPort(unsigned port, EmuTime time) const
{
	switch (port & 3) {
	case 0:
		return index;
	case 1:
		return counters[(index >> 2) & 3].peekReg(index & 3);
	case 2: {
		uint8_t flags = 0;
		for (unsigned i = 0; i < 4; ++i) {
			if (counters[i].peekFlag(time)) flags |= uint8_t(1 << i);
		}
		return flags;
	}
	default:
		return (select < 4) ? counters[select].peekCount(time) : 0xFF;
	}
}

void Y8960Cartridge::Timer::writePort(unsigned port, uint8_t value, EmuTime time)
{
	switch (port & 3) {
	case 0:
		index = value;
		break;
	case 1:
		counters[(index >> 2) & 3].writeReg(index & 3, value, time);
		break;
	case 2:
		for (unsigned i = 0; i < 4; ++i) {
			if (value & (1 << i)) counters[i].clearFlag(time);
		}
		break;
	case 3:
		select = (value & 0xFC) ? 4 : value;
		break;
	}
	updateIrq();
}

void Y8960Cartridge::Timer::updateIrq()
{
	bool any = std::ranges::any_of(counters, &Counter::interrupt);
	if (any) {
		irq.set();
	} else {
		irq.reset();
	}
}


Y8960Cartridge::Timer::Counter::Counter(Timer& timer_, Scheduler& scheduler_)
	: Schedulable(scheduler_)
	, timer(timer_)
{
	clock.setFreq(CLOCK_FREQ);
}

void Y8960Cartridge::Timer::Counter::reset(EmuTime time)
{
	removeSyncPoint();
	clock.reset(time);
	ticks = 0;
	terminal = resolution = 0;
	repeat = intEnable = counting = ended = false;
}

uint8_t Y8960Cartridge::Timer::Counter::peekReg(unsigned reg) const
{
	switch (reg) {
	case 0: return uint8_t((repeat ? 0x01 : 0) | (resolution << 4) | (intEnable ? 0x80 : 0));
	case 1: return terminal;
	case 2: return counting;
	default: return 0;
	}
}

void Y8960Cartridge::Timer::Counter::writeReg(unsigned reg, uint8_t value, EmuTime time)
{
	sync(time);
	switch (reg) {
	case 0:
		repeat = value & 1;
		resolution = (value >> 4) & 7;
		intEnable = value & 0x80;
		break;
	case 1:
		terminal = value;
		break;
	case 2:
		counting = value & 1;
		if (value & 2) ticks = 0;
		break;
	}
	schedule();
}

void Y8960Cartridge::Timer::Counter::clearFlag(EmuTime time)
{
	sync(time);
	ended = false;
	schedule();
}

// The count ends on the last clock of the unit in which it reaches the
// terminal value, or of the current unit if it is past it already. A
// repeating counter starts again from 0, the others stop there.
void Y8960Cartridge::Timer::Counter::advance(uint64_t& t, bool& end, bool& on, uint64_t elapsed) const
{
	while (on && elapsed) {
		uint64_t boundary = uint64_t(std::max(count(t), terminal) + 1) << shift();
		uint64_t step = std::min(boundary - t, elapsed);
		t += step;
		elapsed -= step;
		if (t == boundary) {
			end = true;
			if (repeat) {
				t = 0;
			} else {
				t = boundary - 1;
				on = false;
			}
		}
	}
}

void Y8960Cartridge::Timer::Counter::sync(EmuTime time)
{
	advance(ticks, ended, counting, clock.getTicksTill(time));
	clock.advance(time);
}

uint8_t Y8960Cartridge::Timer::Counter::peekCount(EmuTime time) const
{
	uint64_t t = ticks;
	bool end = ended;
	bool on = counting;
	advance(t, end, on, clock.getTicksTill(time));
	return count(t);
}

bool Y8960Cartridge::Timer::Counter::peekFlag(EmuTime time) const
{
	uint64_t t = ticks;
	bool end = ended;
	bool on = counting;
	advance(t, end, on, clock.getTicksTill(time));
	return end;
}

void Y8960Cartridge::Timer::Counter::schedule()
{
	removeSyncPoint();
	if (counting) {
		uint64_t boundary = uint64_t(std::max(count(ticks), terminal) + 1) << shift();
		setSyncPoint(clock + (boundary - ticks));
	}
}

void Y8960Cartridge::Timer::Counter::executeUntil(EmuTime time)
{
	sync(time);
	schedule();
	timer.updateIrq();
}

template<typename Archive>
void Y8960Cartridge::Timer::Counter::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Schedulable>(*this);
	ar.serialize("clock",      clock,
	             "ticks",      ticks,
	             "terminal",   terminal,
	             "resolution", resolution,
	             "repeat",     repeat,
	             "intEnable",  intEnable,
	             "counting",   counting,
	             "ended",      ended);
}

template<typename Archive>
void Y8960Cartridge::Timer::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("counters", counters,
	             "irq",      irq,
	             "index",    index,
	             "select",   select);
}


template<typename Archive>
void Y8960Cartridge::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Rom8kBBlocks>(*this);
	ar.serialize("scc",           scc,
	             "opll",          opll,
	             "opllLatch",     opllLatch,
	             "opl2",          opl2,
	             "opl2Latch",     opl2Latch,
	             "ssg0",          ssg[0],
	             "ssg1",          ssg[1],
	             "ssgLatch",      ssgLatch,
	             "opm",           opm,
	             "opmLatch",      opmLatch,
	             "timer",         timer,
	             "mixer",         mixer,
	             "ram",           ram,
	             "bankReg",       bankReg,
	             "ramEnabled",    ramEnabled,
	             "boardSelected", boardSelected,
	             "romFunction",   romFunction);
	if constexpr (Archive::IS_LOADER) {
		// the sound devices don't keep their software volume and channel
		// factors in a savestate
		applyAll(getCurrentTime());
	}
	// through the setters, which register the ports that are open
	auto enabler1 = ioEnabler1;
	auto enabler2 = ioEnabler2;
	ar.serialize("ioEnabler1", enabler1,
	             "ioEnabler2", enabler2);
	if constexpr (Archive::IS_LOADER) {
		writeIoEnabler1(enabler1);
		writeIoEnabler2(enabler2);
	}
}
INSTANTIATE_SERIALIZE_METHODS(Y8960Cartridge);
REGISTER_MSXDEVICE(Y8960Cartridge, "Y8960Cartridge");

} // namespace openmsx
