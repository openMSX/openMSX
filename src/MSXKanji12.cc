// $Id$

#include "MSXKanji12.hh"
#include "MSXException.hh"

namespace openmsx {

const byte ID = 0xF7;

MSXKanji12::MSXKanji12(MSXMotherBoard& motherBoard, const XMLElement& config,
                       const EmuTime& time)
	: MSXDevice(motherBoard, config, time)
	, MSXSwitchedDevice(motherBoard, ID)
	, rom(motherBoard, getName(), "Kanji-12 ROM", config)
{
	size = rom.getSize();
	if ((size != 0x20000) && (size != 0x40000)) {
		throw FatalError("MSXKanji12: wrong kanji rom");
	}

	reset(time);
}

MSXKanji12::~MSXKanji12()
{
}

void MSXKanji12::reset(const EmuTime& /*time*/)
{
	adr = 0;	// TODO check this
}

byte MSXKanji12::readIO(byte port, const EmuTime& time)
{
	byte result = peekIO(port, time);
	switch (port & 0x0F) {
		case 9:
			adr++;
			break;
	}
	//PRT_DEBUG("MSXKanji12: read " << (int)port << " " << (int)result);
	return result;
}

byte MSXKanji12::peekIO(byte port, const EmuTime& /*time*/) const
{
	byte result;
	switch (port & 0x0F) {
		case 0:
			result = static_cast<byte>(~ID);
			break;
		case 1:
			result = 0x08;	// TODO what is this
			break;
		case 9:
			if (adr < size) {
				result = rom[adr];
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

void MSXKanji12::writeIO(byte port, byte value, const EmuTime& /*time*/)
{
	//PRT_DEBUG(MSXKanji12: write " << (int)port << " " << (int)value);
	switch (port & 0x0F) {
		case 2:
			// TODO what is this?
			break;
		case 7: {
			byte row = value;
			byte col = ((adr - 0x800) / 18) % 192;
			adr = 0x800 + (row * 192 + col) * 18;
			break;
		}
		case 8: {
			byte row = (adr - 0x800) / (18 * 192);
			byte col = value;
			adr = 0x800 + (row * 192 + col) * 18;
			break;
		}
	}
}

} // namespace openmsx
