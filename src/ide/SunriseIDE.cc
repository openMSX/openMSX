#include "SunriseIDE.hh"
#include "IDEDeviceFactory.hh"
#include "IDEDevice.hh"
#include "Math.hh"
#include "narrow.hh"
#include "outer.hh"
#include "serialize.hh"

namespace openmsx {

SunriseIDE::SunriseIDE(DeviceConfig& config)
	: MSXDevice(config)
	, romBlockDebug(*this)
	, rom(getName() + " ROM", "rom", config)
	, internalBank(subspan<0x4000>(rom)) // make valgrind happy
{
	device[0] = IDEDeviceFactory::create(
		DeviceConfig(config, config.findChild("master")));
	device[1] = IDEDeviceFactory::create(
		DeviceConfig(config, config.findChild("slave")));

	powerUp(getCurrentTime());
}

SunriseIDE::~SunriseIDE() = default;

void SunriseIDE::powerUp(EmuTime time)
{
	writeControl(0xFF);
	reset(time);
}

void SunriseIDE::reset(EmuTime time)
{
	selectedDevice = 0;
	softReset = false;
	device[0]->reset(time);
	device[1]->reset(time);
}

uint8_t SunriseIDE::readMem(uint16_t address, EmuTime time)
{
	if (ideRegsEnabled && ((address & 0x3E00) == 0x3C00)) {
		// 0x7C00 - 0x7DFF   ide data register
		if ((address & 1) == 0) {
			return readDataLow(time);
		} else {
			return readDataHigh(time);
		}
	}
	if (ideRegsEnabled && ((address & 0x3F00) == 0x3E00)) {
		// 0x7E00 - 0x7EFF   ide registers
		return readReg(address & 0xF, time);
	}
	if ((0x4000 <= address) && (address < 0x8000)) {
		// read normal (flash) rom
		return internalBank[address & 0x3FFF];
	}
	// read nothing
	return 0xFF;
}

const uint8_t* SunriseIDE::getReadCacheLine(uint16_t start) const
{
	if (ideRegsEnabled && ((start & 0x3E00) == 0x3C00)) {
		return nullptr;
	}
	if (ideRegsEnabled && ((start & 0x3F00) == 0x3E00)) {
		return nullptr;
	}
	if ((0x4000 <= start) && (start < 0x8000)) {
		return &internalBank[start & 0x3FFF];
	}
	return unmappedRead.data();
}

void SunriseIDE::writeMem(uint16_t address, uint8_t value, EmuTime time)
{
	if ((address & 0xBF04) == 0x0104) {
		// control register
		writeControl(value);
		return;
	}
	if (ideRegsEnabled && ((address & 0x3E00) == 0x3C00)) {
		// 0x7C00 - 0x7DFF   ide data register
		if ((address & 1) == 0) {
			writeDataLow(value);
		} else {
			writeDataHigh(value, time);
		}
		return;
	}
	if (ideRegsEnabled && ((address & 0x3F00) == 0x3E00)) {
		// 0x7E00 - 0x7EFF   ide registers
		writeReg(address & 0xF, value, time);
		return;
	}
	// all other writes ignored
}

uint8_t SunriseIDE::getBank() const
{
	uint8_t bank = Math::reverseByte(control & 0xF8);
	if (bank >= (rom.size() / 0x4000)) {
		bank &= narrow_cast<uint8_t>((rom.size() / 0x4000) - 1);
	}
	return bank;
}

void SunriseIDE::writeControl(uint8_t value)
{
	control = value;
	if (ideRegsEnabled != (control & 1)) {
		ideRegsEnabled = control & 1;
		invalidateDeviceRCache(0x3C00, 0x0300);
		invalidateDeviceRCache(0x7C00, 0x0300);
		invalidateDeviceRCache(0xBC00, 0x0300);
		invalidateDeviceRCache(0xFC00, 0x0300);
	}

	size_t bank = getBank();
	if (internalBank.data() != &rom[0x4000 * bank]) {
		internalBank = subspan<0x4000>(rom, 0x4000 * bank);
		invalidateDeviceRCache(0x4000, 0x4000);
	}
}

uint8_t SunriseIDE::readDataLow(EmuTime time)
{
	uint16_t temp = readData(time);
	readLatch = narrow_cast<uint8_t>(temp >> 8);
	return narrow_cast<uint8_t>(temp & 0xFF);
}
uint8_t SunriseIDE::readDataHigh(EmuTime /*time*/) const
{
	return readLatch;
}
uint16_t SunriseIDE::readData(EmuTime time)
{
	return device[selectedDevice]->readData(time);
}

uint8_t SunriseIDE::readReg(uint4_t reg, EmuTime time)
{
	if (reg == 14) {
		// alternate status register
		reg = 7;
	}
	if (softReset) {
		if (reg == 7) {
			// read status
			return 0xFF; // BUSY
		} else {
			// all others
			return 0x7F; // don't care
		}
	} else {
		if (reg == 0) {
			return narrow_cast<uint8_t>(readData(time) & 0xFF);
		} else {
			uint8_t result = device[selectedDevice]->readReg(reg, time);
			if (reg == 6) {
				result &= 0xEF;
				result |= selectedDevice ? 0x10 : 0x00;
			}
			return result;
		}
	}
}

void SunriseIDE::writeDataLow(uint8_t value)
{
	writeLatch = value;
}
void SunriseIDE::writeDataHigh(uint8_t value, EmuTime time)
{
	auto temp = uint16_t((value << 8) | writeLatch);
	writeData(temp, time);
}
void SunriseIDE::writeData(uint16_t value, EmuTime time)
{
	device[selectedDevice]->writeData(value, time);
}

void SunriseIDE::writeReg(uint4_t reg, uint8_t value, EmuTime time)
{
	if (softReset) {
		if ((reg == 14) && !(value & 0x04)) {
			// clear SRST
			softReset = false;
		}
		// ignore all other writes
	} else {
		if (reg == 0) {
			writeData(narrow_cast<uint16_t>((value << 8) | value), time);
		} else {
			if ((reg == 14) && (value & 0x04)) {
				// set SRST
				softReset = true;
				device[0]->reset(time);
				device[1]->reset(time);
			} else {
				if (reg == 6) {
					selectedDevice = (value & 0x10) ? 1 : 0;
				}
				device[selectedDevice]->writeReg(reg, value, time);
			}
		}
	}
}

template<typename Archive>
void SunriseIDE::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serializePolymorphic("master", *device[0]);
	ar.serializePolymorphic("slave",  *device[1]);
	ar.serialize("readLatch",      readLatch,
	             "writeLatch",     writeLatch,
	             "selectedDevice", selectedDevice,
	             "control",        control,
	             "softReset",      softReset);

	if constexpr (Archive::IS_LOADER) {
		// restore internalBank, ideRegsEnabled
		writeControl(control);
	}
}
INSTANTIATE_SERIALIZE_METHODS(SunriseIDE);
REGISTER_MSXDEVICE(SunriseIDE, "SunriseIDE");


SunriseIDE::Blocks::Blocks(const SunriseIDE& device)
	: RomBlockDebuggableBase(device)
{
}

unsigned SunriseIDE::Blocks::readExt(unsigned address)
{
	if ((address < 0x4000) || (address >= 0x8000)) return unsigned(-1);
	const auto& ide = OUTER(SunriseIDE, romBlockDebug);
	return ide.getBank();
}

} // namespace openmsx
