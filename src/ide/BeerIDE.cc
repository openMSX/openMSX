#include "BeerIDE.hh"

#include "IDEDevice.hh"
#include "IDEDeviceFactory.hh"

#include "GlobalSettings.hh"
#include "serialize.hh"

#include "narrow.hh"

namespace openmsx {

BeerIDE::BeerIDE(DeviceConfig& config)
	: MSXDevice(config)
	, i8255(*this, getCurrentTime(), config.getGlobalSettings().getInvalidPpiModeSetting())
	, rom(getName() + " ROM", "rom", config)
{
	device = IDEDeviceFactory::create(
		DeviceConfig(config, config.findChild("idedevice")));

	powerUp(getCurrentTime());
}

BeerIDE::~BeerIDE() = default;

void BeerIDE::reset(EmuTime time)
{
	controlReg = 0;
	dataReg = 0;
	device->reset(time);
	i8255.reset(time);
}

uint8_t BeerIDE::readMem(uint16_t address, EmuTime /*time*/)
{
	if (0x4000 <= address && address < 0x8000) {
		return rom[address & 0x3FFF];
	}
	return 0xFF;
}

const uint8_t* BeerIDE::getReadCacheLine(uint16_t start) const
{
	if (0x4000 <= start && start < 0x8000) {
		return &rom[start & 0x3FFF];
	}
	return unmappedRead.data();
}

uint8_t BeerIDE::readIO(uint16_t port, EmuTime time)
{
	return i8255.read(port & 0x03, time);
}

uint8_t BeerIDE::peekIO(uint16_t port, EmuTime time) const
{
	return i8255.peek(port & 0x03, time);
}

void BeerIDE::writeIO(uint16_t port, uint8_t value, EmuTime time)
{
	i8255.write(port & 0x03, value, time);
}

// I8255Interface

uint8_t BeerIDE::readA(EmuTime time)
{
	return peekA(time);
}
uint8_t BeerIDE::peekA(EmuTime /*time*/) const
{
	return narrow_cast<uint8_t>(dataReg & 0xFF);
}
void BeerIDE::writeA(uint8_t value, EmuTime /*time*/)
{
	dataReg &= 0xFF00;
	dataReg |= value;
}

uint8_t BeerIDE::readB(EmuTime time)
{
	return peekB(time);
}
uint8_t BeerIDE::peekB(EmuTime /*time*/) const
{
	return narrow_cast<uint8_t>(dataReg >> 8);
}
void BeerIDE::writeB(uint8_t value, EmuTime /*time*/)
{
	dataReg &= 0x00FF;
	dataReg |= (value << 8);
}

uint4_t BeerIDE::readC1(EmuTime time)
{
	return peekC1(time);
}
uint4_t BeerIDE::peekC1(EmuTime /*time*/) const
{
	return 0; // TODO check this
}
uint4_t BeerIDE::readC0(EmuTime time)
{
	return peekC0(time);
}
uint4_t BeerIDE::peekC0(EmuTime /*time*/) const
{
	return 0; // TODO check this
}
void BeerIDE::writeC1(uint4_t value, EmuTime time)
{
	changeControl(uint8_t((controlReg & 0x0F) | (value << 4)), time);
}
void BeerIDE::writeC0(uint4_t value, EmuTime time)
{
	changeControl((controlReg & 0xF0) | value, time);
}
void BeerIDE::changeControl(uint8_t value, EmuTime time)
{
	uint8_t diff = controlReg ^ value;
	controlReg = value;
	if ((diff & 0xE7) == 0) return; // nothing relevant changed

	uint8_t address = controlReg & 7;
	switch (value & 0xE0) {
	case 0x40: // read   /IORD=0,  /IOWR=1,  /CS0=0
		if (address == 0) {
			dataReg = device->readData(time);
		} else {
			dataReg = device->readReg(address, time);
		}
		break;
	case 0x80: // write  /IORD=1,  /IOWR=0,  /CS0=0
		if (address == 0) {
			device->writeData(dataReg, time);
		} else {
			device->writeReg(address, narrow_cast<uint8_t>(dataReg & 0xFF), time);
		}
		break;
	default: // all (6) other cases, nothing
		break;
	}
}

template<typename Archive>
void BeerIDE::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("i8255", i8255);
	ar.serializePolymorphic("device", *device);
	ar.serialize("dataReg",    dataReg,
	             "controlReg", controlReg);
}
INSTANTIATE_SERIALIZE_METHODS(BeerIDE);
REGISTER_MSXDEVICE(BeerIDE, "BeerIDE");

} // namespace openmsx
