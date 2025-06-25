#include "MSXBunsetsu.hh"
#include "CacheLine.hh"
#include "serialize.hh"

namespace openmsx {

MSXBunsetsu::MSXBunsetsu(DeviceConfig& config)
	: MSXDevice(config)
	, bunsetsuRom(getName() + "_1", "rom", config, "bunsetsu")
	, jisyoRom   (getName() + "_2", "rom", config, "jisyo")
{
	reset(EmuTime::dummy());
}

void MSXBunsetsu::reset(EmuTime /*time*/)
{
	jisyoAddress = 0;
}

byte MSXBunsetsu::readMem(uint16_t address, EmuTime /*time*/)
{
	if (address == 0xBFFF) {
		byte result = jisyoRom[jisyoAddress];
		jisyoAddress = (jisyoAddress + 1) & 0x1FFFF;
		return result;
	} else if ((0x4000 <= address) && (address < 0xC000)) {
		return bunsetsuRom[address - 0x4000];
	} else {
		return 0xFF;
	}
}

void MSXBunsetsu::writeMem(uint16_t address, byte value, EmuTime /*time*/)
{
	switch (address) {
	case 0xBFFC:
		jisyoAddress = (jisyoAddress & 0x1FF00) | value;
		break;
	case 0xBFFD:
		jisyoAddress = (jisyoAddress & 0x100FF) | (value << 8);
		break;
	case 0xBFFE:
		jisyoAddress = (jisyoAddress & 0x0FFFF) |
		               ((value & 1) << 16);
		break;
	}
}

const byte* MSXBunsetsu::getReadCacheLine(uint16_t start) const
{
	if ((start & CacheLine::HIGH) == (0xBFFF & CacheLine::HIGH)) {
		return nullptr;
	} else {
		return &bunsetsuRom[start - 0x4000];
	}
}

byte* MSXBunsetsu::getWriteCacheLine(uint16_t start)
{
	if ((start & CacheLine::HIGH) == (0xBFFF & CacheLine::HIGH)) {
		return nullptr;
	} else {
		return unmappedWrite.data();
	}
}

template<typename Archive>
void MSXBunsetsu::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("jisyoAddress", jisyoAddress);
}
INSTANTIATE_SERIALIZE_METHODS(MSXBunsetsu);
REGISTER_MSXDEVICE(MSXBunsetsu, "Bunsetsu");

} // namespace openmsx
