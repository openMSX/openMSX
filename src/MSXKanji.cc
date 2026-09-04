#include "MSXKanji.hh"

#include "MSXException.hh"
#include "TclObject.hh"
#include "serialize.hh"

#include "one_of.hh"

namespace openmsx {

MSXKanji::MSXKanji(DeviceConfig& config)
	: MSXDevice(config)
	, rom(getName(), "Kanji ROM", config)
	, highAddressMask(config.getChildData("type", {}) == "hangul" ? 0x7F : 0x3F)
{
	auto level2ViaStr = config.getChildData("level_2_via", "read"); // default 'read' for bw compat
	if (level2ViaStr == "read") {
		level2Via = Level2Via::Read;
		portMask = 3;
	} else if (level2ViaStr == "write") {
		level2Via = Level2Via::Write;
		portMask = 3;
	} else if (level2ViaStr == "interlocked_write_read") {
		level2Via = Level2Via::InterlockedWriteRead;
		portMask = 3;
	} else if (level2ViaStr == "only_level_1") {
		level2Via = Level2Via::Read;
		portMask = 1; // implemented by redirecting level2 ports to level1
	} else {
		throw MSXException("MSXKanji: invalid value for 'level_2_via'");
	}

	auto size = rom.size();
	if (size != one_of(0x20000u, 0x40000u)) {
		throw MSXException("MSXKanji: wrong kanji ROM, it should be either 128kB or 256kB.");
	}
	if ((highAddressMask == 0x7F) && (size != 0x40000)) {
		throw MSXException("MSXKanji: for hangul type, the font ROM must be 256kB.");
	}

	reset(EmuTime::dummy());
}

void MSXKanji::reset(EmuTime /*time*/)
{
	adr = 0; // TODO check inital value
	writeLevel = 0;
}

void MSXKanji::writeIO(uint16_t port, uint8_t value, EmuTime /*time*/)
{
	port &= portMask;
	writeLevel = (port & 2) ? 1 : 0;

	switch (port & 1) {
	case 0:
		adr = (adr & 0x3f800) | ((value & 0x3f) << 5); // 6 bit column
		break;
	case 1:
		adr = (adr & 0x007e0) | ((value & highAddressMask) << 11); // 6 or 7 (for 'hangul') bit row
		break;
	}
	// note: write to either row/column resets the counter
}

MSXKanji::ReadImplResult MSXKanji::readImpl(uint16_t port) const
{
	uint8_t readLevel = (port & portMask & 2) ? 1 : 0;

	unsigned address = 0;
	switch (level2Via) {
	case Level2Via::Read:
		address = adr | (readLevel << 17);
		break;
	case Level2Via::InterlockedWriteRead:
		if (readLevel != writeLevel) return {.valid = false, .value = 0xFF};
		[[fallthrough]];
	case Level2Via::Write:
		address = adr | (writeLevel << 17);
		break;
	}

	auto value = (address < rom.size()) ? rom[address] : uint8_t(0xff);
	return {.valid = true, .value = value};
}

uint8_t MSXKanji::peekIO(uint16_t port, EmuTime /*time*/) const
{
	return readImpl(port).value;
}

uint8_t MSXKanji::readIO(uint16_t port, EmuTime /*time*/)
{
	auto [valid, value] = readImpl(port);
	if (valid) {
		adr = (adr & ~0x1f) | ((adr + 1) & 0x1f);
	}
	return value;
}

void MSXKanji::getExtraDeviceInfo(TclObject& result) const
{
	rom.getInfo(result);
}

// version 1 : initial version
// version 2 : replaced adr1/adr2 with a single shared adr, added writeLevel
template<typename Archive>
void MSXKanji::serialize(Archive& ar, unsigned version)
{
	ar.template serializeBase<MSXDevice>(*this);
	if (ar.versionBelow(version, 2)) {
		assert(Archive::IS_LOADER);
		ar.serialize("adr1", adr);
		//ar.serialize("adr2", adr); // ignored
		writeLevel = 0;
	} else {
		ar.serialize("adr",        adr,
		             "writeLevel", writeLevel);
	}
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
