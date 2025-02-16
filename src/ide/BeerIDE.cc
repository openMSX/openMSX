#include "BeerIDE.hh"
#include "IDEDeviceFactory.hh"
#include "IDEDevice.hh"
#include "GlobalSettings.hh"
#include "narrow.hh"
#include "serialize.hh"

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

void BeerIDE::reset(EmuTime::param time)
{
	controlReg = 0;
	dataReg = 0;
	device->reset(time);
	i8255.reset(time);
}

byte BeerIDE::readMem(word address, EmuTime::param /*time*/)
{
	if (0x4000 <= address && address < 0x8000) {
		return rom[address & 0x3FFF];
	}
	return 0xFF;
}

const byte* BeerIDE::getReadCacheLine(word start) const
{
	if (0x4000 <= start && start < 0x8000) {
		return &rom[start & 0x3FFF];
	}
	return unmappedRead.data();
}

byte BeerIDE::readIO(word port, EmuTime::param time)
{
	return i8255.read(port & 0x03, time);
}

byte BeerIDE::peekIO(word port, EmuTime::param time) const
{
	return i8255.peek(port & 0x03, time);
}

void BeerIDE::writeIO(word port, byte value, EmuTime::param time)
{
	i8255.write(port & 0x03, value, time);
}

// I8255Interface

byte BeerIDE::readA(EmuTime::param time)
{
	return peekA(time);
}
byte BeerIDE::peekA(EmuTime::param /*time*/) const
{
	return narrow_cast<byte>(dataReg & 0xFF);
}
void BeerIDE::writeA(byte value, EmuTime::param /*time*/)
{
	dataReg &= 0xFF00;
	dataReg |= value;
}

byte BeerIDE::readB(EmuTime::param time)
{
	return peekB(time);
}
byte BeerIDE::peekB(EmuTime::param /*time*/) const
{
	return narrow_cast<byte>(dataReg >> 8);
}
void BeerIDE::writeB(byte value, EmuTime::param /*time*/)
{
	dataReg &= 0x00FF;
	dataReg |= (value << 8);
}

nibble BeerIDE::readC1(EmuTime::param time)
{
	return peekC1(time);
}
nibble BeerIDE::peekC1(EmuTime::param /*time*/) const
{
	return 0; // TODO check this
}
nibble BeerIDE::readC0(EmuTime::param time)
{
	return peekC0(time);
}
nibble BeerIDE::peekC0(EmuTime::param /*time*/) const
{
	return 0; // TODO check this
}
void BeerIDE::writeC1(nibble value, EmuTime::param time)
{
	changeControl(byte((controlReg & 0x0F) | (value << 4)), time);
}
void BeerIDE::writeC0(nibble value, EmuTime::param time)
{
	changeControl((controlReg & 0xF0) | value, time);
}
void BeerIDE::changeControl(byte value, EmuTime::param time)
{
	byte diff = controlReg ^ value;
	controlReg = value;
	if ((diff & 0xE7) == 0) return; // nothing relevant changed

	byte address = controlReg & 7;
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
			device->writeReg(address, narrow_cast<byte>(dataReg & 0xFF), time);
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
