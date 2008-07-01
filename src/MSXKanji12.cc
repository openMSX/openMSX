// $Id$

#include "MSXKanji12.hh"
#include "Rom.hh"
#include "MSXException.hh"
#include "serialize.hh"

namespace openmsx {

static const byte ID = 0xF7;

MSXKanji12::MSXKanji12(MSXMotherBoard& motherBoard, const XMLElement& config)
	: MSXDevice(motherBoard, config)
	, MSXSwitchedDevice(motherBoard, ID)
	, rom(new Rom(motherBoard, getName(), "Kanji-12 ROM", config))
{
	unsigned size = rom->getSize();
	if ((size != 0x20000) && (size != 0x40000)) {
		throw MSXException("MSXKanji12: wrong kanji rom");
	}

	reset(*static_cast<EmuTime*>(0));
}

MSXKanji12::~MSXKanji12()
{
}

void MSXKanji12::reset(const EmuTime& /*time*/)
{
	address = 0; // TODO check this
}

byte MSXKanji12::readSwitchedIO(word port, const EmuTime& time)
{
	byte result = peekIO(port, time);
	switch (port & 0x0F) {
		case 9:
			address++;
			break;
	}
	//PRT_DEBUG("MSXKanji12: read " << (int)port << " " << (int)result);
	return result;
}

byte MSXKanji12::peekSwitchedIO(word port, const EmuTime& /*time*/) const
{
	byte result;
	switch (port & 0x0F) {
		case 0:
			result = byte(~ID);
			break;
		case 1:
			result = 0x08; // TODO what is this
			break;
		case 9:
			if (address < rom->getSize()) {
				result = (*rom)[address];
			} else {
				result = 0xFF;
			}
			break;
		default:
			result = 0xFF;
			break;
	}
	return result;
}

void MSXKanji12::writeSwitchedIO(word port, byte value, const EmuTime& /*time*/)
{
	//PRT_DEBUG(MSXKanji12: write " << (int)port << " " << (int)value);
	switch (port & 0x0F) {
		case 2:
			// TODO what is this?
			break;
		case 7: {
			byte row = value;
			byte col = ((address - 0x800) / 18) % 192;
			address = 0x800 + (row * 192 + col) * 18;
			break;
		}
		case 8: {
			byte row = (address - 0x800) / (18 * 192);
			byte col = value;
			address = 0x800 + (row * 192 + col) * 18;
			break;
		}
	}
}

template<typename Archive>
void MSXKanji12::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	// no need to serialize MSXSwitchedDevice base class

	ar.serialize("address", address);
}
INSTANTIATE_SERIALIZE_METHODS(MSXKanji12);

} // namespace openmsx
