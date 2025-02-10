#include "FraelSwitchableROM.hh"
#include "serialize.hh"

/*
 * Information as reverse engineered by FRS:
 *
 * Curiously, the machine works without the boot-logo+firmware ROM, because the
 * MainBIOS checks for its presence. If it's not there, it just skips the logo and
 * the menu. Maybe they were planning a cheaper version without that ROM.
 *
 * The boot-logo+firmware ROM is selected by the bit7 of the I/O port 90h. This is
 * the same port that has the strobe bit (bit-1) in normal MSX computers. Besides
 * the extra bit, the rest of the printer port seems to be pretty ordinary.
 *
 * Bit7=0: Select Frael MainBIOS ROM on slot-0
 * Bit7=1: Select the boot-logo+firmware ROM on slot-0
 */

namespace openmsx {

FraelSwitchableROM::FraelSwitchableROM(DeviceConfig& config)
	: MSXDevice(config)
	, basicBiosRom(getName() + " BASIC/BIOS", "rom", config, "basicbios")
	, firmwareRom (getName() + " firmware"  , "rom", config, "firmware" )
{
	reset(EmuTime::dummy());
}

void FraelSwitchableROM::writeIO(word /*port*/, byte value, EmuTime::param /*time*/)
{
	bool newValue = value & 0x80;
	if (newValue != firmwareSelected) {
		firmwareSelected = newValue;
		invalidateDeviceRCache();
	}
}

void FraelSwitchableROM::reset(EmuTime::param /*time*/)
{
	firmwareSelected = false;
	invalidateDeviceRCache();
}

byte FraelSwitchableROM::readMem(word address, EmuTime::param /*time*/)
{
	return *getReadCacheLine(address);
}

const byte* FraelSwitchableROM::getReadCacheLine(word start) const
{
	return firmwareSelected ? &firmwareRom[start & (firmwareRom.size() - 1)] :
		&basicBiosRom[start & (basicBiosRom.size() - 1)];
}

template<typename Archive>
void FraelSwitchableROM::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("firmwareSelected", firmwareSelected);
}
INSTANTIATE_SERIALIZE_METHODS(FraelSwitchableROM);
REGISTER_MSXDEVICE(FraelSwitchableROM, "FraelSwitchableROM");

} // namespace openmsx
