// $Id$

#include "MSXKanji12.hh"


const byte ID = 0xF7;

MSXKanji12::MSXKanji12(Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXSwitchedDevice(ID),
	  rom(config, time)
{
	int size = rom.getSize();
	if ((size != 0x20000) && (size != 0x40000)) {
		PRT_ERROR("MSXKanji12: wrong kanji rom");
	}
	mask = size - 1;

	reset(time);
}

MSXKanji12::~MSXKanji12()
{
}

void MSXKanji12::reset(const EmuTime &time)
{
	adr = 0;	// TODO check this
}

byte MSXKanji12::readIO(byte port, const EmuTime &time)
{
	byte result;
	switch (port & 0x0F) {
		case 0:
			result = (byte)~ID;
			break;
		case 1:
			result = 0x08;	// TODO what is this
			break;
		case 9:
			result = rom.read(adr);
			adr = (adr + 1) & mask;
			break;
		default:
			result = 0xFF;
			break;
	}
	//PRT_DEBUG("MSXKanji12: read " << (int)port << " " << (int)result);
	return result;
}

void MSXKanji12::writeIO(byte port, byte value, const EmuTime &time)
{
	//PRT_DEBUG(MSXKanji12: write " << (int)port << " " << (int)value);
	switch (port & 0x0F) {
		case 2:
			// TODO what is this?
			break;
		case 7: {
			byte row = value;
			byte col = ((adr - 0x800) / 18) % 192;
			adr = (0x800 + (row * 192 + col) * 18) & mask;
			break;
		}
		case 8: {
			byte row = (adr - 0x800) / (18 * 192);
			byte col = value;
			adr = (0x800 + (row * 192 + col) * 18) & mask;
			break;
		}
	}
}
