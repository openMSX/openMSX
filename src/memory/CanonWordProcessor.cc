// EC701 - Japanese Word Processor Unit (Konami)
// Reverse engineered by Albert Beevendorp.
// Probably identical to Canon VWU-100.
//
// Select register in range 0xbff8-0xbfff (0xbfff used by software).
// This select the memory visible in area 0x4000-0x7fff.
// The value written here is inverted, then:
//   bit 5-3 = chip select:
//        000: program ROM (lower 16 kB)
//        100: dictionary ROM   0-128 kB
//        101: dictionary ROM 128-256 kB
//        110: dictionary ROM 256-384 kB
//     others: not mapped (reads as 0xc0)
//   bit 2-0 = block select within dictionary ROM
//
// Area 0x8000-0xbff7 reads from program ROM (upper 16 kB)
//
// Also check the electrical schema on:
//   http://d4.princess.ne.jp/msx/other/ec700/index.htm

#include "CanonWordProcessor.hh"
#include "MSXException.hh"
#include "CacheLine.hh"
#include "serialize.hh"

namespace openmsx {

CanonWordProcessor::CanonWordProcessor(const DeviceConfig& config)
	: MSXDevice(config)
	, programRom   (strCat(config.getAttributeValue("id"), "_program"),    "rom", config, "program")
	, dictionaryRom(strCat(config.getAttributeValue("id"), "_dictionary"), "rom", config, "dictionary")
{
	if (programRom.size() != 32*1024) {
		throw MSXException("Program ROM must be 32kB.");
	}
	if (dictionaryRom.size() != 3*128*1024) {
		throw MSXException("Dictionary ROM must be 3x 128kB.");
	}
	reset(EmuTime::dummy());
}

void CanonWordProcessor::reset(EmuTime::param time)
{
	writeMem(0xbfff, 0xff, time);
}

byte CanonWordProcessor::readMem(word address, EmuTime::param time)
{
	return peekMem(address, time);
}

byte CanonWordProcessor::peekMem(word address, EmuTime::param /*time*/) const
{
	if ((0xbff8 <= address) && (address <= 0xbfff)) {
		return 0xc0; // select area, does NOT read from ROM
	} else {
		if (const auto* a = readHelper(address)) {
			return *a;
		}
		return 0xc0;
	}
}

const byte* CanonWordProcessor::getReadCacheLine(word start) const
{
	if ((start & CacheLine::HIGH) == (0xbfff & CacheLine::HIGH)) {
		return nullptr; // select register
	} else {
		return readHelper(start);
	}
}

const byte* CanonWordProcessor::readHelper(word address) const
{
	if ((0x4000 <= address) && (address <= 0x7fff)) {
		auto chip = (select & 0b00'111'000) >> 3;
		auto bank = (select & 0b00'011'111);
		address &= 0x3fff;
		switch (chip) {
		case 0b000:
			return &programRom[address];
		case 0b100: case 0b101: case 0b110:
			return &dictionaryRom[0x4000 * bank + address];
		// note: according to the schematic on
		// http://d4.princess.ne.jp/msx/other/ec700/index.htm, the
		// outputs of IC5's 3-to-8 decoder for '001' '010' '011' and
		// '111' are unconnected
		}
		return nullptr; // unmapped, but reads as 0xc0 instead of 0xff ??
	} else {
		// 0x8000 - 0xbfff
		return &programRom[0x4000 + (address & 0x3fff)];
	}
}

void CanonWordProcessor::writeMem(word address, byte value, EmuTime::param /*time*/)
{
	if ((0xbff8 <= address) && (address <= 0xbfff)) {
		select = ~value; // invert !!
		invalidateDeviceRCache(0x4000, 0x4000);
	}
}

byte* CanonWordProcessor::getWriteCacheLine(word /*start*/) const
{
	return nullptr;
}

template<typename Archive>
void CanonWordProcessor::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("select", select);
}
INSTANTIATE_SERIALIZE_METHODS(CanonWordProcessor);
REGISTER_MSXDEVICE(CanonWordProcessor, "CanonWordProcessor");

} // namespace openmsx
