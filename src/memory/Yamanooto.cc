#include "Yamanooto.hh"

#include "DummyAY8910Periphery.hh"
#include "MSXCPUInterface.hh"

#include "narrow.hh"
#include "outer.hh"
#include "ranges.hh"
#include "unreachable.hh"
#include "view.hh"

#include <cassert>

// TODO:
// * HOME/DEL boot keys.  How are these handled?
// * SCC stereo mode.

namespace openmsx {

static constexpr uint16_t ENAR = 0x7FFF;
static constexpr uint8_t  REGEN  = 0x01;
static constexpr uint8_t  WREN   = 0x10;
static constexpr uint16_t OFFR = 0x7FFE;
static constexpr uint16_t CFGR = 0x7FFD;
static constexpr uint8_t  MDIS   = 0x01;
static constexpr uint8_t  ECHO   = 0x02;
static constexpr uint8_t  ROMDIS = 0x04;
static constexpr uint8_t  K4     = 0x08;
static constexpr uint8_t  SUBOFF = 0x30;
// undocumented stuff
static constexpr uint8_t  FPGA_EN   = 0x40; // write: 1 -> enable communication with FPGA commands ???
static constexpr uint8_t  FPGA_WAIT = 0x80; // read: ready signal ??? (1 = ready)
static constexpr uint16_t FPGA_REG  = 0x7FFC; // bi-direction 8-bit communication channel

Yamanooto::Yamanooto(DeviceConfig& config, Rom&& rom_)
	: MSXRom(config, std::move(rom_))
	, romBlockDebug(*this)
	, flash(rom, AmdFlashChip::S29GL064N90TFI04, {}, config)
	, scc(getName() + " SCC", config, getCurrentTime(), SCC::Mode::Compatible)
	, psg(getName() + " PSG", DummyAY8910Periphery::instance(), config, getCurrentTime())
{
	// Tests show that the PSG has higher volume than the scc. See
	//   https://www.msx.org/forum/msx-talk/openmsx/openmsx-cartridge-type?page=2#comment-470014
	//   https://msx.pics/images/2024/12/23/yamanooto_volume_comparison.png
	psg.setSoftwareVolume(3.0f, getCurrentTime());

	scc.setBalance(0,  0.00f);
	scc.setBalance(1, -0.67f);
	scc.setBalance(2,  0.67f);
	scc.setBalance(3, -0.67f);
	scc.setBalance(4,  0.67f);
	scc.postSetBalance();

	getCPUInterface().register_IO_Out_range(0x10, 2, this);
	powerUp(getCurrentTime());
}

Yamanooto::~Yamanooto()
{
	writeConfigReg(0); // unregister A0-A2 ports
	getCPUInterface().unregister_IO_Out_range(0x10, 2, this);
}

void Yamanooto::powerUp(EmuTime time)
{
	scc.powerUp(time);
	reset(time);
}

void Yamanooto::reset(EmuTime time)
{
	// TODO is offsetReg changed by reset?
	// TODO are all bits in configReg set to zero?   (also bit 0,1)
	enableReg = 0;
	offsetReg = 0;
	writeConfigReg(0);
	fpgaFsm = 0;

	ranges::iota(bankRegs, uint16_t(0));
	ranges::iota(rawBanks, uint8_t(0));
	sccMode = 0;
	scc.reset(time);

	psgLatch = 0;
	psg.reset(time);

	flash.reset();

	invalidateDeviceRCache(); // flush all to be sure
}

void Yamanooto::writeConfigReg(byte value)
{
	if ((value ^ configReg) & ECHO) {
		if (value & ECHO) {
			getCPUInterface().register_IO_Out_range(0xA0, 2, this);
		} else {
			getCPUInterface().unregister_IO_Out_range(0xA0, 2, this);
		}
	}
	configReg = value;
}

bool Yamanooto::isSCCAccess(uint16_t address) const
{
	if (configReg & K4) return false; // Konami4 doesn't have SCC

	// This uses the raw bank registers (not adjusted with offset register),
	//   see: https://github.com/openMSX/openMSX/issues/1992
	if (sccMode & 0x20) {
		// SCC+   range: 0xB800..0xBFFF,  excluding 0xBFFE-0xBFFF
		return  (rawBanks[3] & 0x80)          && (0xB800 <= address) && (address < 0xBFFE);
	} else {
		// SCC    range: 0x9800..0x9FFF
		return ((rawBanks[2] & 0x3F) == 0x3F) && (0x9800 <= address) && (address < 0xA000);
	}
}

unsigned Yamanooto::getFlashAddr(unsigned addr) const
{
	unsigned page8kB = (addr >> 13) - 2;
	assert(page8kB < 4); // must be inside [0x4000, 0xBFFF]
	auto bank = bankRegs[page8kB] & 0x3ff; // max 8MB
	return (bank << 13) | (addr & 0x1FFF);
}

[[nodiscard]] static uint16_t mirror(uint16_t address)
{
	if (address < 0x4000 || 0xC000 <= address) {
		// mirror 0x4000 <-> 0xc000   /   0x8000 <-> 0x0000
		address ^= 0x8000; // real hw probably just ignores upper address bit
	}
	assert((0x4000 <= address) && (address < 0xC000));
	return address;
}

static constexpr std::array<byte, 4 + 1> FPGA_ID = {
	0xFF, // idle
	0x1F, 0x23, 0x00, 0x00, // TODO check last 2
};
byte Yamanooto::peekMem(uint16_t address, EmuTime time) const
{
	address = mirror(address);

	// 0x7ffc-0x7fff
	if (FPGA_REG <= address && address <= ENAR && (enableReg & REGEN)) {
		switch (address) {
		case FPGA_REG:
			if (!(configReg & FPGA_EN)) return 0xFF; // TODO check this
			assert(fpgaFsm < 5);
			return FPGA_ID[fpgaFsm];
		case CFGR: return configReg | FPGA_WAIT; // not-busy
		case OFFR: return offsetReg;
		case ENAR: return enableReg;
		default: UNREACHABLE;
		}
	}

	// 0x9800-0x9FFF or 0xB800-0xBFFD
	if (isSCCAccess(address)) {
		return scc.peekMem(narrow_cast<uint8_t>(address & 0xFF), time);
	}
	return ((configReg & ROMDIS) == 0) ? flash.peek(getFlashAddr(address), time)
	                                   : 0xFF; // access to flash ROM disabled
}

byte Yamanooto::readMem(uint16_t address, EmuTime time)
{
	// 0x7ffc-0x7fff  (NOT mirrored)
	if (FPGA_REG <= address && address <= ENAR && (enableReg & REGEN)) {
		return peekMem(address, time);
	}
	address = mirror(address);

	// 0x9800-0x9FFF or 0xB800-0xBFFD
	if (isSCCAccess(address)) {
		return scc.readMem(narrow_cast<uint8_t>(address & 0xFF), time);
	}
	return ((configReg & ROMDIS) == 0) ? flash.read(getFlashAddr(address), time)
	                                   : 0xFF; // access to flash ROM disabled
}

const byte* Yamanooto::getReadCacheLine(uint16_t address) const
{
	if (((address & CacheLine::HIGH) == (ENAR & CacheLine::HIGH)) && (enableReg & REGEN)) {
		return nullptr; // Yamanooto registers, non-cacheable
	}
	address = mirror(address);
	if (isSCCAccess(address)) return nullptr;
	return ((configReg & ROMDIS) == 0) ? flash.getReadCacheLine(getFlashAddr(address))
	                                   : unmappedRead.data(); // access to flash ROM disabled
}

void Yamanooto::writeMem(uint16_t address, byte value, EmuTime time)
{
	// 0x7ffc-0x7fff  (NOT mirrored)
	if (FPGA_REG <= address && address <= ENAR) {
		if (address == ENAR) {
			enableReg = value;
		} else if (enableReg & REGEN) {
			switch (address) {
			case FPGA_REG:
				if (!(configReg & FPGA_EN)) break;
				switch (fpgaFsm) {
				case 0:
					if (value == 0x9f) { // read ID command ??
						++fpgaFsm;
					}
					break;
				case 1: case 2: case 3:
					++fpgaFsm;
					break;
				case 4:
					fpgaFsm = 0;
					break;
				default:
					UNREACHABLE;
				}
				break;
			case CFGR:
				writeConfigReg(value);
				break;
			case OFFR:
				offsetReg = value; // does NOT immediately switch bankRegs
				break;
			default:
				UNREACHABLE;
			}
		}
		invalidateDeviceRCache();
	}
	address = mirror(address);
	unsigned page8kB = (address >> 13) - 2;

	if (enableReg & WREN) {
		// write to flash rom
		if (!(configReg & ROMDIS)) { // disabled?
			flash.write(getFlashAddr(address), value, time);
			invalidateDeviceRCache(); // needed ?
		}
	} else {
		auto invalidateCache = [&](unsigned start, unsigned size) {
			invalidateDeviceRCache(start, size);
			invalidateDeviceRCache(start ^ 0x8000, size); // mirror
		};
		auto offset = (offsetReg << 2) | ((configReg & SUBOFF) >> 4);
		if (configReg & K4) {
			// Konami-4
			if (((configReg & MDIS) == 0) && (0x6000 <= address)) {
				// bank 0 is NOT switchable, but it keeps the
				// value it had before activating K4 mode
				bankRegs[page8kB] = (value + offset) & 0x3FF;
				rawBanks[page8kB] = value;
				invalidateCache(0x4000 + 0x2000 * page8kB, 0x2000);
			}
		} else {
			// 0x9800-0x9FFF or 0xB800-0xBFFD
			if (isSCCAccess(address)) {
				scc.writeMem(narrow_cast<uint8_t>(address & 0xFF), value, time);
			}

			// Konami-SCC
			if (((address & 0x1800) == 0x1000) && ((configReg & MDIS) == 0)) {
				// [0x5000,0x57FF] [0x7000,0x77FF]
				// [0x9000,0x97FF] [0xB000,0xB7FF]
				bankRegs[page8kB] = (value + offset) & 0x3FF;
				rawBanks[page8kB] = value;
				invalidateCache(0x4000 + 0x2000 * page8kB, 0x2000);
			}

			// SCC mode register
			if ((address & 0xFFFE) == 0xBFFE) {
				sccMode = value;
				scc.setMode((value & 0x20) ? SCC::Mode::Plus
				                           : SCC::Mode::Compatible);
				invalidateCache(0x9800, 0x800);
				invalidateCache(0xB800, 0x800);
			}
		}
	}
}

byte* Yamanooto::getWriteCacheLine(uint16_t /*address*/)
{
	return nullptr;
}

byte Yamanooto::peekIO(uint16_t /*port*/, EmuTime /*time*/) const
{
	return 0xff;
}

byte Yamanooto::readIO(uint16_t /*port*/, EmuTime /*time*/)
{
	// PSG is not readable
	return 0xff; // should never be called
}

void Yamanooto::writeIO(uint16_t port, byte value, EmuTime time)
{
	if (port & 1) { // 0x11 or 0xA1
		psg.writeRegister(psgLatch, value, time);
	} else { // 0x10 or 0xA0
		psgLatch = value & 0x0F;
	}
}

// version 1: initial version
// version 2: added 'rawBanks'
template<typename Archive>
void Yamanooto::serialize(Archive& ar, unsigned version)
{
	ar.serialize(
		"flash", flash,
		"scc", scc,
		"psg", psg,
		"bankRegs", bankRegs,
		"enableReg", enableReg,
		"offsetReg", offsetReg,
		"configReg", configReg,
		"sccMode", sccMode,
		"psgLatch", psgLatch,
		"fpgaFsm", fpgaFsm
	);
	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("rawBanks", rawBanks);
	} else {
		// Could be wrong, but the best we can do?
		auto offset = (offsetReg << 2) | ((configReg & SUBOFF) >> 4);
		for (auto [b, r] : view::zip(bankRegs, rawBanks)) {
			r = uint8_t(b - offset);
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(Yamanooto);
REGISTER_MSXDEVICE(Yamanooto, "Yamanooto");


unsigned Yamanooto::Blocks::readExt(unsigned address)
{
	const auto& dev = OUTER(Yamanooto, romBlockDebug);
	address = mirror(narrow<uint16_t>(address));
	unsigned page8kB = (address >> 13) - 2;
	return dev.bankRegs[page8kB];
}

} // namespace openmsx
