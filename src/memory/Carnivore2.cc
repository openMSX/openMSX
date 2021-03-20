#include "Carnivore2.hh"
#include "IDEDevice.hh"
#include "IDEDeviceFactory.hh"
#include "MSXCPU.hh"
#include "cstd.hh"
#include "one_of.hh"
#include "ranges.hh"
#include "xrange.hh"
#include <array>

// TODO (besides what's in the code below):
// - behaviour of non unique values in configReg[0x28] is not properly
//   implemented, but also not supported. So print a warning when that condition
//   occurs. See e.g. https://godbolt.org/z/j6e7MW
// - FM-PAC mono/stereo setting (bit 7)
// - possibly the PPI volume setting
// - slave slot support

namespace openmsx {

static constexpr auto sectorInfo = [] {
	// 8 * 8kB, followed by 127 * 64kB
	using Info = AmdFlash::SectorInfo;
	std::array<Info, 8 + 127> result = {};
	cstd::fill(result.begin(), result.begin() + 8, Info{ 8 * 1024, false});
	cstd::fill(result.begin() + 8, result.end(),   Info{64 * 1024, false});
	return result;
}();

Carnivore2::Carnivore2(const DeviceConfig& config)
	: MSXDevice(config)
	, MSXMapperIOClient(getMotherBoard())
	, flash("Carnivore2 flash", sectorInfo, 0x207e,
	        AmdFlash::Addressing::BITS_12, config)
	, ram(config, getName() + " ram", "ram", 2048 * 1024)
	, eeprom(getName() + " eeprom",
	         DeviceConfig(config, config.getChild("eeprom")))
	, scc(getName() + " scc", config, getCurrentTime(), SCC::SCC_Compatible)
	, ym2413(getName() + " ym2413", config)
{
	ideDevices[0] = IDEDeviceFactory::create(
		DeviceConfig(config, config.findChild("master")));
	ideDevices[1] = IDEDeviceFactory::create(
		DeviceConfig(config, config.findChild("slave")));
}

Carnivore2::~Carnivore2() = default;

void Carnivore2::powerUp(EmuTime::param time)
{
	writeSndLVL(0x1b, time);
	scc.powerUp(time);
	reset(time);
}

void Carnivore2::reset(EmuTime::param time)
{
	subSlotReg = 0;
	port3C = 0;

	// config regs
	configRegs[0x00] = 0x30; // CardMDR
	for (int i : {0x01, 0x02, 0x03}) configRegs[i] = 0; // AddrM<i>
	configRegs[0x05] = shadowConfigRegs[0x05] = 0; // AddrFR

	configRegs[0x06] = shadowConfigRegs[0x06] = 0xF8; // R1Mask
	configRegs[0x07] = shadowConfigRegs[0x07] = 0x50; // R1Addr
	configRegs[0x08] = shadowConfigRegs[0x08] = 0x00; // R1Reg
	configRegs[0x09] = shadowConfigRegs[0x09] = 0x85; // R1Mult
	configRegs[0x0a] = shadowConfigRegs[0x0a] = 0x03; // B1MaskR
	configRegs[0x0b] = shadowConfigRegs[0x0b] = 0x40; // B1AdrD

	for (int i : {0x0f, 0x15, 0x1b}) {
		configRegs[i] = shadowConfigRegs[i] = 0; // R<i>Mult
	}

	configRegs[0x1e] = shadowConfigRegs[0x1e] = 0xff;
	configRegs[0x20] = 0x02;
	configRegs[0x28] = 0b11'10'01'00; // SLM_cfg

	writeCfgEEPR(0, time);

	// multi-mapper
	scc.reset(time);
	sccMode = 0;
	ranges::iota(sccBank, 0);

	// ide
	ideControlReg = 0;
	ideSelectedDevice = 0;
	ideSoftReset = false;
	ideDevices[0]->reset(time);
	ideDevices[1]->reset(time);

	// memory mapper
	ranges::iota(memMapRegs, 0); // Note: different from how BIOS initializes these registers

	// fm-pac
	ym2413.reset(time);
	fmPacEnable = 0x10;
	fmPacBank = 0;
	fmPac5ffe = 0;
	fmPac5fff = 0;
}

void Carnivore2::globalRead(word address, EmuTime::param /*time*/)
{
	if (!delayedConfig()) return;

	if ((!delayedConfig4000() && (address == 0x0000) && (getCPU().isM1Cycle(address))) ||
	    ( delayedConfig4000() && (address <= 0x4000) && (address < 0x4010))) {
		// activate delayed configuration
		for (auto i : xrange(0x05, 0x1f)) {
			configRegs[i] = shadowConfigRegs[i];
		}
	}
}

Carnivore2::SubDevice Carnivore2::getSubDevice(word address) const
{
	byte subSlot(-1);

	if (slotExpanded()) {
		byte page = address >> 14;
		byte selectedSubSlot = (subSlotReg >> (2 * page)) & 0x03;
		if (subSlotEnabled(selectedSubSlot)) {
			subSlot = selectedSubSlot;
		}
	} else {
		for (auto i : xrange(4)) {
			if (subSlotEnabled(i)) {
				subSlot = i;
				break;
			}
		}
	}

	if        (subSlot == (configRegs[0x28] & 0b00'00'00'11) >> 0) {
		return SubDevice::MultiMapper;
	} else if (subSlot == (configRegs[0x28] & 0b00'00'11'00) >> 2) {
		return SubDevice::IDE;
	} else if (subSlot == (configRegs[0x28] & 0b00'11'00'00) >> 4) {
		return SubDevice::MemoryMapper;
	} else if (subSlot == (configRegs[0x28] & 0b11'00'00'00) >> 6) {
		return SubDevice::FmPac;
	} else {
		return SubDevice::Nothing;
	}
}

byte Carnivore2::readMem(word address, EmuTime::param time)
{
	if (slotExpanded() && (address == 0xffff)) {
		return subSlotReg ^ 0xff;
	}
	switch (getSubDevice(address)) {
		case SubDevice::MultiMapper:  return readMultiMapperSlot(address, time);
		case SubDevice::IDE:          return readIDESlot(address, time);
		case SubDevice::MemoryMapper: return readMemoryMapperSlot(address);
		case SubDevice::FmPac:        return readFmPacSlot(address);
		default:                      return 0xff;
	}
}

byte Carnivore2::peekMem(word address, EmuTime::param time) const
{
	if (slotExpanded() && (address == 0xffff)) {
		return subSlotReg ^ 0xff;
	}
	switch (getSubDevice(address)) {
		case SubDevice::MultiMapper:  return peekMultiMapperSlot(address, time);
		case SubDevice::IDE:          return peekIDESlot(address, time);
		case SubDevice::MemoryMapper: return peekMemoryMapperSlot(address);
		case SubDevice::FmPac:        return peekFmPacSlot(address);
		default:                      return 0xff;
	}
}

void Carnivore2::writeMem(word address, byte value, EmuTime::param time)
{
	if (slotExpanded() && (address == 0xffff)) {
		subSlotReg = value;
		// this does not block the writes below
	}

	switch (getSubDevice(address)) {
		case SubDevice::MultiMapper:
			writeMultiMapperSlot(address, value, time);
			break;
		case SubDevice::IDE:
			writeIDESlot(address, value, time);
			break;
		case SubDevice::MemoryMapper:
			writeMemoryMapperSlot(address, value);
			break;
		case SubDevice::FmPac:
			writeFmPacSlot(address, value, time);
			break;
		default:
			// unmapped, do nothing
			break;
	}
}

unsigned Carnivore2::getDirectFlashAddr() const
{
	return (configRegs[0x01] <<  0) |
	       (configRegs[0x02] <<  8) |
	       (configRegs[0x03] << 16);  // already masked to 7 bits
}

byte Carnivore2::peekConfigRegister(word address, EmuTime::param time) const
{
	address &= 0x3f;
	if ((0x05 <= address) && (address <= 0x1e)) {
		// only these registers have a shadow counterpart,
		// reads happen from the shadowed version
		return shadowConfigRegs[address];
	} else {
		switch (address) {
			case 0x04: return flash.peek(getDirectFlashAddr());
			case 0x1f: return configRegs[0x00]; // mirror 'CardMDR' register
			case 0x23: return configRegs[address] |
					  int(eeprom.read_DO(time));
			case 0x2C: return '2';
			case 0x2D: return '3';
			case 0x2E: return '0';
			default:   return configRegs[address];
		}
	}
}

byte Carnivore2::readConfigRegister(word address, EmuTime::param time)
{
	address &= 0x3f;
	if (address == 0x04) {
		return flash.read(getDirectFlashAddr());
	} else {
		return peekConfigRegister(address, time);
	}
}

static constexpr float volumeLevel(byte volume)
{
	constexpr byte tab[8] = {5, 6, 7, 8, 10, 12, 14, 16};
	return tab[volume & 7] / 16.0f;
}

void Carnivore2::writeSndLVL(byte value, EmuTime::param time)
{
	configRegs[0x22] = value;
	ym2413.setSoftwareVolume(volumeLevel(value >> 3), time);
	scc   .setSoftwareVolume(volumeLevel(value >> 0), time);
}

void Carnivore2::writeCfgEEPR(byte value, EmuTime::param time)
{
	configRegs[0x23] = value & 0x0e;
	eeprom.write_DI (value & 2, time);
	eeprom.write_CLK(value & 4, time);
	eeprom.write_CS (value & 8, time);
}

void Carnivore2::writeConfigRegister(word address, byte value, EmuTime::param time)
{
	address &= 0x3f;
	if ((0x05 <= address) && (address <= 0x1e)) {
		// shadow registers
		if (address == 0x05) value &= 0x7f;
		if ((address == 0x1e) && ((value & 0x8f) == 0x0f)) return; // ignore write

		shadowConfigRegs[address] = value;
		if (!delayedConfig()) configRegs[address] = value;
	} else {
		switch (address) {
			case 0x03: configRegs[address] = value & 0x7f; break;
			case 0x04: flash.write(getDirectFlashAddr(), value); break;
			case 0x1f: configRegs[0x00] = value; break; // mirror 'CardMDR' register
			case 0x20: configRegs[address] = value & 0x07; break;
			case 0x22: writeSndLVL(value, time); break;
			case 0x23: writeCfgEEPR(value, time); break;
			//case 0x24: // TODO PSG key-click
			default:   configRegs[address] = value; break;
		}
	}
}

bool Carnivore2::isConfigReg(word address) const
{
	if (configRegs[0x00] & 0x80) return false; // config regs disabled
	unsigned base = ((configRegs[0x00] & 0x60) << 9) | 0xF80;
	return (base <= address) && (address < (base + 0x40));
}

std::pair<unsigned, byte> Carnivore2::decodeMultiMapper(word address) const
{
	// check up to 4 possible banks
	for (auto i : xrange(4)) {
		const byte* base = configRegs + (i * 6) + 6; // points to R<i>Mask
		byte mult = base[3];
		if (mult & 8) continue; // bank disabled

		byte sizeCode = mult & 7;
		if (sizeCode < 3) continue; // invalid size

		// check address
		bool mirroringDisabled = mult & 0x40;
		static constexpr byte checkMasks[2][8] = {
			{ 0x00, 0x00, 0x00, 0x30, 0x60, 0xc0, 0x80, 0x00 }, // mirroring enabled
			{ 0x00, 0x00, 0x00, 0xf0, 0xe0, 0xc0, 0x80, 0x00 }, // mirroring disabled
		};
		byte checkMask = checkMasks[mirroringDisabled][sizeCode];
		if (((address >> 8) & checkMask) != (base[5] & checkMask)) continue;

		// found bank
		byte bank = base[2] & base[4];
		unsigned size = 512 << sizeCode; // 7->64k, 6->32k, ..., 3->4k
		unsigned addr = (bank * size) | (address & (size - 1));
		addr += configRegs[0x05] * 0x10000; // 64kB block offset
		addr &= 0x7fffff; // 8MB
		return {addr, mult};
	}
	return {unsigned(-1), byte(-1)};
}

bool Carnivore2::sccAccess(word address) const
{
	if (!sccEnabled()) return false;
	if (sccMode & 0x20) {
		// check scc plus
		return (0xb800 <= address) && (address < 0xc000) &&
		       ((sccBank[3] & 0x80) == 0x80);
	} else {
		// check scc compatible
		return (0x9800 <= address) && (address < 0xa000) &&
	               ((sccBank[2] & 0x3f) == 0x3f);
	}
}

byte Carnivore2::readMultiMapperSlot(word address, EmuTime::param time)
{
	if (isConfigReg(address)) {
		return readConfigRegister(address, time);
	}
	if (sccAccess(address)) {
		return scc.readMem(address & 0xff, time);
	}

	auto [addr, mult] = decodeMultiMapper(address);
	if (addr == unsigned(-1)) return 0xff; // unmapped

	if (mult & 0x20) {
		return ram[addr & 0x1fffff]; // 2MB
	} else {
		return flash.read(addr);
	}
}

byte Carnivore2::peekMultiMapperSlot(word address, EmuTime::param time) const
{
	if (isConfigReg(address)) {
		return peekConfigRegister(address, time);
	}

	auto [addr, mult] = decodeMultiMapper(address);
	if (addr == unsigned(-1)) return 0xff; // unmapped

	if (mult & 0x20) {
		return ram[addr & 0x1fffff]; // 2MB
	} else {
		return flash.peek(addr);
	}
}

void Carnivore2::writeMultiMapperSlot(word address, byte value, EmuTime::param time)
{
	if (isConfigReg(address)) {
		// this blocks writes to switch-region and bank-region
		return writeConfigRegister(address, value, time);
	}

	// check (all) 4 bank switch regions
	for (auto i : xrange(4)) {
		byte* base = configRegs + (i * 6) + 6; // points to R<i>Mask
		byte mask = base[0];
		byte addr = base[1];
		byte mult = base[3];
		if (mult & 0x80) { // enable bit in R<i>Mult
			if (((address >> 8) & mask) == (addr & mask)) {
				// update actual+shadow reg
				      configRegs[(i * 6) + 6 + 2] = value;
				shadowConfigRegs[(i * 6) + 6 + 2] = value;
			}
		}
	}

	auto [addr, mult] = decodeMultiMapper(address);
	if ((addr != unsigned(-1)) && (mult & 0x10)) { // write enable
		if (mult & 0x20) {
			ram[addr & 0x1fffff] = value; // 2MB
		} else {
			flash.write(addr, value);
		}
	}

	if (sccEnabled() && ((address | 1) == 0xbfff)) {
		// write scc mode register (write-only)
		sccMode = value;
		scc.setChipMode((sccMode & 0x20) ? SCC::SCC_plusmode : SCC::SCC_Compatible);
	}
	if (((sccMode & 0x10) == 0x00) && // note: no check for sccEnabled()
	    ((address & 0x1800) == 0x1000)) {
		byte region = (address >> 13) - 2;
		sccBank[region] = value;
	} else if (sccAccess(address)) {
		scc.writeMem(address & 0xff, value, time);
	}
}

byte Carnivore2::readIDESlot(word address, EmuTime::param time)
{
	// TODO mirroring is different from SunriseIDE
	if (ideRegsEnabled() && ((address & 0xfe00) == 0x7c00)) {
		// 0x7c00-0x7dff   IDE data register
		switch (address & 1) {
			case 0: { // data low
				auto tmp = ideReadData(time);
				ideRead = tmp >> 8;
				return tmp & 0xff;
			}
			case 1: // data high
				return ideRead;
		}
	}
	if (ideRegsEnabled() && ((address & 0xff00) == 0x7e00)) {
		// 0x7e00-0x7eff   IDE registers
		return ideReadReg(address & 0xf, time);
	}
	if ((0x4000 <= address) && (address < 0x8000)) {
		// read IDE flash rom
		unsigned addr = (address & 0x3fff) + (ideBank() * 0x4000) + 0x10000;
		if (readBIOSfromRAM()) {
			return ram[addr];
		} else {
			return flash.read(addr);
		}
	}
	return 0xff;
}

byte Carnivore2::peekIDESlot(word address, EmuTime::param /*time*/) const
{
	if (ideRegsEnabled() && ((address & 0xfe00) == 0x7c00)) {
		// 0x7c00-0x7dff   IDE data register
		return 0xff; // TODO not yet implemented
	}
	if (ideRegsEnabled() && ((address & 0xff00) == 0x7e00)) {
		// 0x7e00-0x7eff   IDE registers
		return 0xff; // TODO not yet implemented
	}
	if ((0x4000 <= address) && (address < 0x8000)) {
		// read IDE flash rom
		unsigned addr = (address & 0x3fff) + (ideBank() * 0x4000) + 0x10000;
		if (readBIOSfromRAM()) {
			return ram[addr];
		} else {
			return flash.peek(addr);
		}
	}
	return 0xff;
}

void Carnivore2::writeIDESlot(word address, byte value, EmuTime::param time)
{
	// TODO mirroring is different from SunriseIDE
	if (address == 0x4104) {
		ideControlReg = value;

	} else if (ideRegsEnabled() && ((address & 0xfe00) == 0x7c00)) {
		// 0x7c00-0x7dff   IDE data register
		switch (address & 1) {
			case 0: // data low
				ideWrite = value;
				break;
			case 1: { // data high
				word tmp = (value << 8) | ideWrite;
				ideWriteData(tmp, time);
				break;
			}
		}

	} else if (ideRegsEnabled() && ((address & 0xff00) == 0x7e00)) {
		// 0x7e00-0x7eff   IDE registers
		ideWriteReg(address & 0xf, value, time);
	}
}

word Carnivore2::ideReadData(EmuTime::param time)
{
	return ideDevices[ideSelectedDevice]->readData(time);
}

void Carnivore2::ideWriteData(word value, EmuTime::param time)
{
	ideDevices[ideSelectedDevice]->writeData(value, time);
}

byte Carnivore2::ideReadReg(byte reg, EmuTime::param time)
{
	if (reg == 14) reg = 7; // alternate status register

	if (ideSoftReset) {
		if (reg == 7) { // read status
			return 0xff; // busy
		} else { // all others
			return 0x7f; // don't care
		}
	} else {
		if (reg == 0) {
			return ideReadData(time) & 0xff;
		} else {
			auto result = ideDevices[ideSelectedDevice]->readReg(reg, time);
			if (reg == 6) {
				result = (result & 0xef) | (ideSelectedDevice ? 0x10 : 0x00);
			}
			return result;
		}
	}
}

void Carnivore2::ideWriteReg(byte reg, byte value, EmuTime::param time)
{
	if (ideSoftReset) {
		if ((reg == 14) && !(value & 0x04)) {
			// clear SRST
			ideSoftReset = false;
		}
		// ignore all other writes
	} else {
		if (reg == 0) {
			ideWriteData((value << 8) | value, time);
		} else {
			if ((reg == 14) && (value & 0x04)) {
				// set SRST
				ideSoftReset = true;
				ideDevices[0]->reset(time);
				ideDevices[1]->reset(time);
			} else {
				if (reg == 6) {
					ideSelectedDevice = (value & 0x10) ? 1 : 0;
				}
				ideDevices[ideSelectedDevice]->writeReg(reg, value, time);
			}
		}
	}
}

bool Carnivore2::isMemmapControl(word address) const
{
	return (port3C & 0x80) &&
	       (( (port3C & 0x08) && ((address & 0xc000) == 0x4000)) ||
	        (!(port3C & 0x08) && ((address & 0xc000) == 0x8000)));
}

unsigned Carnivore2::getMemoryMapperAddress(word address) const
{
	return (address & 0x3fff) +
	       0x4000 * memMapRegs[address >> 14] +
	       0x100000; // 2nd half of 2MB
}

bool Carnivore2::isMemoryMapperWriteProtected(word address) const
{
	byte page = address >> 14;
	return (port3C & (1 << page)) != 0;
}

byte Carnivore2::peekMemoryMapperSlot(word address) const
{
	if (isMemmapControl(address)) {
		switch (address & 0xff) {
		case 0x3c:
			return port3C;
		case 0xfc: case 0xfd: case 0xfe: case 0xff:
			return memMapRegs[address & 0x03];
		}
	}
	return ram[getMemoryMapperAddress(address)];
}

byte Carnivore2::readMemoryMapperSlot(word address)
{
	return peekMemoryMapperSlot(address);
}

void Carnivore2::writeMemoryMapperSlot(word address, byte value)
{
	if (isMemmapControl(address)) {
		switch (address & 0xff) {
		case 0x3c:
			value |= (value & 0x02) << 6; // TODO should be '(.. 0x20) << 2' ???
			port3C = value;
			return;
		case 0xfc: case 0xfd: case 0xfe: case 0xff:
			memMapRegs[address & 0x03] = value & 0x3f;
			return;
		}
	}
	if (!isMemoryMapperWriteProtected(address)) {
		ram[getMemoryMapperAddress(address)] = value;
	}
}

byte Carnivore2::readFmPacSlot(word address)
{
	if (address == 0x7ff6) {
		return fmPacEnable; // enable
	} else if (address == 0x7ff7) {
		return fmPacBank; // bank
	} else if ((0x4000 <= address) && (address < 0x8000)) {
		if (fmPacSramEnabled()) {
			if (address < 0x5ffe) {
				return ram[(address & 0x1fff) | 0xfe000];
			} else if (address == 0x5ffe) {
				return fmPac5ffe; // always 0x4d
			} else if (address == 0x5fff) {
				return fmPac5fff; // always 0x69
			} else {
				return 0xff;
			}
		} else {
			unsigned addr = (address & 0x3fff) + (0x4000 * fmPacBank) + 0x30000;
			if (readBIOSfromRAM()) {
				return ram[addr];
			} else {
				return flash.read(addr);
			}
		}
	}
	return 0xff;
}

byte Carnivore2::peekFmPacSlot(word address) const
{
	if (address == 0x7ff6) {
		return fmPacEnable; // enable
	} else if (address == 0x7ff7) {
		return fmPacBank; // bank
	} else if ((0x4000 <= address) && (address < 0x8000)) {
		if (fmPacSramEnabled()) {
			if (address < 0x5ffe) {
				return ram[(address & 0x1fff) | 0xfe000];
			} else if (address == 0x5ffe) {
				return fmPac5ffe; // always 0x4d
			} else if (address == 0x5fff) {
				return fmPac5fff; // always 0x69
			} else {
				return 0xff;
			}
		} else {
			unsigned addr = (address & 0x3fff) + (0x4000 * fmPacBank) + 0x30000;
			if (readBIOSfromRAM()) {
				return ram[addr];
			} else {
				return flash.peek(addr);
			}
		}
	}
	return 0xff;
}

void Carnivore2::writeFmPacSlot(word address, byte value, EmuTime::param time)
{
	if ((0x4000 <= address) && (address < 0x5ffe)) {
		if (fmPacSramEnabled()) {
			ram[(address & 0x1fff) | 0xfe000] = value;
		}
	} else if (address == 0x5ffe) {
		fmPac5ffe = value;
	} else if (address == 0x5fff) {
		fmPac5fff = value;
	} else if (address == one_of(0x7ff4, 0x7ff5)) {
		ym2413.writePort(address & 1, value, time);
	} else if (address == 0x7ff6) {
		fmPacEnable = value & 0x11;
	} else if (address == 0x7ff7) {
		fmPacBank = value & 0x03;
	}
}

byte Carnivore2::readIO(word port, EmuTime::param time)
{
	return peekIO(port, time);
}

byte Carnivore2::peekIO(word port, EmuTime::param /*time*/) const
{
	// reading ports 0x3c, 0x7c, 0x7d has no effect
	if (memMapReadEnabled() && ((port & 0xfc) == 0xfc)) {
		// memory mapper registers
		return getSelectedSegment(port & 3);
	}
	return 0xff;
}

void Carnivore2::writeIO(word port, byte value, EmuTime::param time)
{
	if (((port & 0xfe) == 0x7c) &&
	    (fmPacPortEnabled1() || fmPacPortEnabled2())) {
		// fm-pac registers
		ym2413.writePort(port & 1, value, time);
	} else if (((port & 0xff) == 0x3c) && writePort3cEnabled()) {
		// only change bit 7
		port3C = (port3C & 0x7F) | (value & 0x80);

	} else if ((port & 0xfc) == 0xfc) {
		// memory mapper registers
		memMapRegs[port & 0x03] = value & 0x3f;
		invalidateDeviceRWCache(0x4000 * (port & 0x03), 0x4000);
	}
}

byte Carnivore2::getSelectedSegment(byte page) const
{
	return memMapRegs[page];
}

// version 1: initial version
// version 2: removed fmPacRegSelect
template<typename Archive>
void Carnivore2::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("flash",            flash,
	             "ram",              ram,
	             "eeprom",           eeprom,
	             "configRegs",       configRegs,
	             "shadowConfigRegs", shadowConfigRegs,
	             "subSlotReg",       subSlotReg,
	             "port3C",           port3C,

	             "scc",              scc,
	             "sccMode",          sccMode,
	             "sccBank",          sccBank);

	ar.serializePolymorphic("master", *ideDevices[0]);
	ar.serializePolymorphic("slave",  *ideDevices[1]);
	ar.serialize("ideSoftReset",      ideSoftReset,
	             "ideSelectedDevice", ideSelectedDevice,
	             "ideControlReg",     ideControlReg,
	             "ideRead",           ideRead,
	             "ideWrite",          ideWrite,

	             "memMapRegs",        memMapRegs,

	             "ym2413",            ym2413,
	             "fmPacEnable",       fmPacEnable,
	             "fmPacBank",         fmPacBank,
	             "fmPac5ffe",         fmPac5ffe,
	             "fmPac5fff",         fmPac5fff);

	if constexpr (Archive::IS_LOADER) {
		auto time = getCurrentTime();
		writeSndLVL (configRegs[0x22], time);
		writeCfgEEPR(configRegs[0x23], time);
	}
}
INSTANTIATE_SERIALIZE_METHODS(Carnivore2);
REGISTER_MSXDEVICE(Carnivore2, "Carnivore2");

} // namespace openmsx
