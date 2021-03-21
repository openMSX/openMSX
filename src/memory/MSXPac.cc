#include "MSXPac.hh"
#include "CacheLine.hh"
#include "serialize.hh"

namespace openmsx {

constexpr const char* const PAC_Header = "PAC2 BACKUP DATA";

MSXPac::MSXPac(const DeviceConfig& config)
	: MSXDevice(config)
	, sram(getName() + " SRAM", 0x1FFE, config, PAC_Header)
{
	reset(EmuTime::dummy());
}

void MSXPac::reset(EmuTime::param /*time*/)
{
	sramEnabled = false;
	r1ffe = r1fff = 0xFF; // TODO check
}

byte MSXPac::readMem(word address, EmuTime::param /*time*/)
{
	address &= 0x3FFF;
	if (sramEnabled) {
		if (address < 0x1FFE) {
			return sram[address];
		} else if (address == 0x1FFE) {
			return r1ffe;
		} else if (address == 0x1FFF) {
			return r1fff;
		} else {
			return 0xFF;
		}
	} else {
		return 0xFF;
	}
}

const byte* MSXPac::getReadCacheLine(word address) const
{
	address &= 0x3FFF;
	if (sramEnabled) {
		if (address < (0x1FFE & CacheLine::HIGH)) {
			return &sram[address];
		} else if (address == (0x1FFE & CacheLine::HIGH)) {
			return nullptr;
		} else {
			return unmappedRead;
		}
	} else {
		return unmappedRead;
	}
}

void MSXPac::writeMem(word address, byte value, EmuTime::param /*time*/)
{
	address &= 0x3FFF;
	switch (address) {
		case 0x1FFE:
			r1ffe = value;
			checkSramEnable();
			break;
		case 0x1FFF:
			r1fff = value;
			checkSramEnable();
			break;
		default:
			if (sramEnabled && (address < 0x1FFE)) {
				sram.write(address, value);
			}
	}
}

byte* MSXPac::getWriteCacheLine(word address) const
{
	address &= 0x3FFF;
	if (address == (0x1FFE & CacheLine::HIGH)) {
		return nullptr;
	}
	if (sramEnabled && (address < 0x1FFE)) {
		return nullptr;
	} else {
		return unmappedWrite;
	}
}

void MSXPac::checkSramEnable()
{
	bool newEnabled = (r1ffe == 0x4D) && (r1fff == 0x69);
	if (sramEnabled != newEnabled) {
		sramEnabled = newEnabled;
		invalidateDeviceRWCache();
	}
}

template<typename Archive>
void MSXPac::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("SRAM",  sram,
	             "r1ffe", r1ffe,
	             "r1fff", r1fff);
	if constexpr (ar.IS_LOADER) {
		checkSramEnable();
	}
}
INSTANTIATE_SERIALIZE_METHODS(MSXPac);
REGISTER_MSXDEVICE(MSXPac, "PAC");

} // namespace openmsx
