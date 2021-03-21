#include "SunriseIDE.hh"
#include "IDEDeviceFactory.hh"
#include "IDEDevice.hh"
#include "Math.hh"
#include "serialize.hh"
#include "outer.hh"

namespace openmsx {

SunriseIDE::SunriseIDE(const DeviceConfig& config)
	: MSXDevice(config)
	, romBlockDebug(*this)
	, rom(getName() + " ROM", "rom", config)
{
	device[0] = IDEDeviceFactory::create(
		DeviceConfig(config, config.findChild("master")));
	device[1] = IDEDeviceFactory::create(
		DeviceConfig(config, config.findChild("slave")));

	// make valgrind happy
	internalBank = nullptr;
	ideRegsEnabled = false;

	powerUp(getCurrentTime());
}

SunriseIDE::~SunriseIDE() = default;

void SunriseIDE::powerUp(EmuTime::param time)
{
	writeControl(0xFF);
	reset(time);
}

void SunriseIDE::reset(EmuTime::param time)
{
	selectedDevice = 0;
	softReset = false;
	device[0]->reset(time);
	device[1]->reset(time);
}

byte SunriseIDE::readMem(word address, EmuTime::param time)
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

const byte* SunriseIDE::getReadCacheLine(word start) const
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
	return unmappedRead;
}

void SunriseIDE::writeMem(word address, byte value, EmuTime::param time)
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

byte SunriseIDE::getBank() const
{
	byte bank = Math::reverseByte(control & 0xF8);
	if (bank >= (rom.getSize() / 0x4000)) {
		bank &= ((rom.getSize() / 0x4000) - 1);
	}
	return bank;
}

void SunriseIDE::writeControl(byte value)
{
	control = value;
	if (ideRegsEnabled != (control & 1)) {
		ideRegsEnabled = control & 1;
		invalidateDeviceRCache(0x3C00, 0x0300);
		invalidateDeviceRCache(0x7C00, 0x0300);
		invalidateDeviceRCache(0xBC00, 0x0300);
		invalidateDeviceRCache(0xFC00, 0x0300);
	}

	byte bank = getBank();
	if (internalBank != &rom[0x4000 * bank]) {
		internalBank = &rom[0x4000 * bank];
		invalidateDeviceRCache(0x4000, 0x4000);
	}
}

byte SunriseIDE::readDataLow(EmuTime::param time)
{
	word temp = readData(time);
	readLatch = temp >> 8;
	return temp & 0xFF;
}
byte SunriseIDE::readDataHigh(EmuTime::param /*time*/) const
{
	return readLatch;
}
word SunriseIDE::readData(EmuTime::param time)
{
	return device[selectedDevice]->readData(time);
}

byte SunriseIDE::readReg(nibble reg, EmuTime::param time)
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
			return readData(time) & 0xFF;
		} else {
			byte result = device[selectedDevice]->readReg(reg, time);
			if (reg == 6) {
				result &= 0xEF;
				result |= selectedDevice ? 0x10 : 0x00;
			}
			return result;
		}
	}
}

void SunriseIDE::writeDataLow(byte value)
{
	writeLatch = value;
}
void SunriseIDE::writeDataHigh(byte value, EmuTime::param time)
{
	word temp = (value << 8) | writeLatch;
	writeData(temp, time);
}
void SunriseIDE::writeData(word value, EmuTime::param time)
{
	device[selectedDevice]->writeData(value, time);
}

void SunriseIDE::writeReg(nibble reg, byte value, EmuTime::param time)
{
	if (softReset) {
		if ((reg == 14) && !(value & 0x04)) {
			// clear SRST
			softReset = false;
		}
		// ignore all other writes
	} else {
		if (reg == 0) {
			writeData((value << 8) | value, time);
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

	if constexpr (ar.IS_LOADER) {
		// restore internalBank, ideRegsEnabled
		writeControl(control);
	}
}
INSTANTIATE_SERIALIZE_METHODS(SunriseIDE);
REGISTER_MSXDEVICE(SunriseIDE, "SunriseIDE");


SunriseIDE::Blocks::Blocks(SunriseIDE& device)
	: RomBlockDebuggableBase(device)
{
}

byte SunriseIDE::Blocks::read(unsigned address)
{
	if ((address < 0x4000) || (address >= 0x8000)) return 255;
	auto& ide = OUTER(SunriseIDE, romBlockDebug);
	return ide.getBank();
}

} // namespace openmsx
