#include "MSXKanji12.hh"

#include "MSXException.hh"
#include "TclObject.hh"
#include "serialize.hh"

#include "narrow.hh"
#include "one_of.hh"

namespace openmsx {

static constexpr byte ID = 0xF7;

MSXKanji12::MSXKanji12(DeviceConfig& config)
	: MSXDevice(config)
	, MSXSwitchedDevice(getMotherBoard(), ID)
	, rom(getName(), "Kanji-12 ROM", config)
{
	if (rom.size() != one_of(0x20000u, 0x40000u)) {
		throw MSXException("MSXKanji12: wrong kanji ROM, it should be either 128kB or 256kB.");
	}

	reset(EmuTime::dummy());
}

void MSXKanji12::reset(EmuTime /*time*/)
{
	address = 0; // TODO check this
}

byte MSXKanji12::readSwitchedIO(uint16_t port, EmuTime time)
{
	byte result = peekSwitchedIO(port, time);
	switch (port & 0x0F) {
		case 9:
			address++;
			break;
	}
	return result;
}

byte MSXKanji12::peekSwitchedIO(uint16_t port, EmuTime /*time*/) const
{
	switch (port & 0x0F) {
		case 0:
			return byte(~ID);
		case 1:
			return 0x08; // TODO what is this
		case 9:
			if (address < rom.size()) {
				return rom[address];
			} else {
				return 0xFF;
			}
		default:
			return 0xFF;
	}
}

void MSXKanji12::writeSwitchedIO(uint16_t port, byte value, EmuTime /*time*/)
{
	switch (port & 0x0F) {
		case 2:
			// TODO what is this?
			break;
		case 7: {
			byte row = value;
			byte col = narrow<byte>(((address - 0x800) / 18) % 192);
			address = 0x800 + (row * 192 + col) * 18;
			break;
		}
		case 8: {
			byte row = narrow<byte>((address - 0x800) / (18 * 192));
			byte col = value;
			address = 0x800 + (row * 192 + col) * 18;
			break;
		}
	}
}

void MSXKanji12::getExtraDeviceInfo(TclObject& result) const
{
	rom.getInfo(result);
}

template<typename Archive>
void MSXKanji12::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	// no need to serialize MSXSwitchedDevice base class

	ar.serialize("address", address);
}
INSTANTIATE_SERIALIZE_METHODS(MSXKanji12);
REGISTER_MSXDEVICE(MSXKanji12, "Kanji12");

} // namespace openmsx
