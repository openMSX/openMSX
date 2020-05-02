#include "MSXKanji.hh"
#include "MSXException.hh"
#include "one_of.hh"
#include "serialize.hh"

namespace openmsx {

MSXKanji::MSXKanji(const DeviceConfig& config)
	: MSXDevice(config)
	, rom(getName(), "Kanji ROM", config)
	, isLascom(config.getChildData("type", {}) == "lascom")
	, highAddressMask(config.getChildData("type", {}) == "hangul" ? 0x7F : 0x3F)
{
	int size = rom.getSize();
	if (size != one_of(0x20000, 0x40000)) {
		throw MSXException("MSXKanji: wrong kanji ROM, it should be either 128kB or 256kB.");
	}
	if ((highAddressMask == 0x7F) && (size != 0x40000)) {
		throw MSXException("MSXKanji: for hangul type, the font ROM must be 256kB.");
	}

	reset(EmuTime::dummy());
}

void MSXKanji::reset(EmuTime::param /*time*/)
{
	adr1 = 0x00000; // TODO check this
	adr2 = 0x20000; // TODO check this
}

void MSXKanji::writeIO(word port, byte value, EmuTime::param /*time*/)
{
	switch (port & 0x03) {
	case 0:
		adr1 = (adr1 & 0x1f800) | ((value & 0x3f) << 5);
		break;
	case 1:
		adr1 = (adr1 & 0x007e0) | ((value & highAddressMask) << 11);
		break;
	case 2:
		adr2 = (adr2 & 0x3f800) | ((value & 0x3f) << 5);
		break;
	case 3:
		adr2 = (adr2 & 0x207e0) | ((value & 0x3f) << 11);
		break;
	}
}

byte MSXKanji::readIO(word port, EmuTime::param time)
{
	byte result = peekIO(port, time);
	switch (port & 0x03) {
	case 0:
		if (!isLascom) {
			break;
		}
		[[fallthrough]];
	case 1:
		adr1 = (adr1 & ~0x1f) | ((adr1 + 1) & 0x1f);
		break;
	case 3:
		adr2 = (adr2 & ~0x1f) | ((adr2 + 1) & 0x1f);
		break;
	}
	return result;
}

byte MSXKanji::peekIO(word port, EmuTime::param /*time*/) const
{
	byte result = 0xff;
	switch (port & 0x03) {
	case 0:
		if (!isLascom) {
			break;
		}
		[[fallthrough]];
	case 1:
		result = rom[adr1 & (rom.getSize() - 1)]; // mask to be safe
		break;
	case 3:
		if (rom.getSize() == 0x40000) { // temp workaround
			result = rom[adr2];
		}
		break;
	}
	return result;
}

template<typename Archive>
void MSXKanji::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("adr1", adr1,
	             "adr2", adr2);
}
INSTANTIATE_SERIALIZE_METHODS(MSXKanji);
REGISTER_MSXDEVICE(MSXKanji, "Kanji");

/*
This really works!

10 DIM A(32)
20 FOR I=0 TO 4095
30 OUT &HD8, I MOD 64: OUT &HD9, I\64
40 FOR J=0 TO 31: A(J)=INP(&HD9): NEXT
50 FOR J=0 TO 7
60 PRINT RIGHT$("0000000"+BIN$(A(J)), 8);
70 PRINT RIGHT$("0000000"+BIN$(A(J+8)), 8)
80 NEXT
90 FOR J=16 TO 23
100 PRINT RIGHT$("0000000"+BIN$(A(J)), 8);
110 PRINT RIGHT$("0000000"+BIN$(A(J+8)), 8)
120 NEXT
130 PRINT
140 NEXT
*/

} // namespace openmsx
