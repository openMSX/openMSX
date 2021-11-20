// Holy Qu'ran  cartridge
//  It is like an ASCII 8KB, but using the 5000h, 5400h, 5800h and 5C00h
//  addresses.
//
// This is very similar to RomHolyQuran, but this mapper type works with the
// encrypted ROM content. Thanks to n_n for implementing it in meisei and
// sharing his implementation with us (and pointing us to it).

#include "RomHolyQuran2.hh"
#include "MSXCPU.hh"
#include "MSXException.hh"
#include "enumerate.hh"
#include "likely.hh"
#include "outer.hh"
#include "ranges.hh"
#include "serialize.hh"
#include <array>

namespace openmsx {

// protection uses a simple rotation on databus, some lines inverted:
//   out0 = ~in3   out1 =  in7   out2 = ~in5   out3 = ~in1
//   out4 =  in0   out5 =  in4   out6 = ~in2   out7 =  in6
static constexpr auto decryptLUT = [] {
	std::array<byte, 256> result = {};
	//for (auto [i, r] : enumerate(result)) { msvc bug
	for (int i = 0; i < 256; ++i) {
		result[i] = (((i << 4) & 0x50) |
		             ((i >> 3) & 0x05) |
		             ((i << 1) & 0xa0) |
		             ((i << 2) & 0x08) |
	                     ((i >> 6) & 0x02)) ^ 0x4d;
	}
	return result;
}();

RomHolyQuran2::RomHolyQuran2(const DeviceConfig& config, Rom&& rom_)
	: MSXRom(config, std::move(rom_))
	, romBlocks(*this)
{
	if (rom.getSize() != 0x100000) { // 1MB
		throw MSXException("Holy Quaran ROM should be exactly 1MB in size");
	}
	reset(EmuTime::dummy());
}

void RomHolyQuran2::reset(EmuTime::param /*time*/)
{
	ranges::fill(bank, &rom[0]);
	decrypt = false;
}

byte RomHolyQuran2::readMem(word address, EmuTime::param time)
{
	byte result = RomHolyQuran2::peekMem(address, time);
	if (unlikely(!decrypt)) {
		if (getCPU().isM1Cycle(address)) {
			// start decryption when we start executing the rom
			decrypt = true;
		}
	}
	return result;
}

byte RomHolyQuran2::peekMem(word address, EmuTime::param /*time*/) const
{
	if ((0x4000 <= address) && (address < 0xc000)) {
		unsigned b = (address - 0x4000) >> 13;
		byte raw = bank[b][address & 0x1fff];
		return decrypt ? decryptLUT[raw] : raw;
	} else {
		return 0xff;
	}
}

void RomHolyQuran2::writeMem(word address, byte value, EmuTime::param /*time*/)
{
	// TODO are switch addresses mirrored?
	if ((0x5000 <= address) && (address < 0x6000)) {
		byte region = (address >> 10) & 3;
		bank[region] = &rom[(value & 127) * 0x2000];
	}
}

const byte* RomHolyQuran2::getReadCacheLine(word address) const
{
	if ((0x4000 <= address) && (address < 0xc000)) {
		return nullptr;
	} else {
		return unmappedRead;
	}
}

byte* RomHolyQuran2::getWriteCacheLine(word address) const
{
	if ((0x5000 <= address) && (address < 0x6000)) {
		return nullptr;
	} else {
		return unmappedWrite;
	}
}

template<typename Archive>
void RomHolyQuran2::serialize(Archive& ar, unsigned /*version*/)
{
	// skip MSXRom base class
	ar.template serializeBase<MSXDevice>(*this);

	unsigned bb[4];
	if constexpr (Archive::IS_LOADER) {
		ar.serialize("banks", bb);
		for (auto [i, b] : enumerate(bb)) {
			bank[i] = &rom[(b & 127) * 0x2000];
		}
	} else {
		for (auto [i, b] : enumerate(bb)) {
			b = (bank[i] - &rom[0]) / 0x2000;
		}
		ar.serialize("banks", bb);
	}

	ar.serialize("decrypt", decrypt);
}
INSTANTIATE_SERIALIZE_METHODS(RomHolyQuran2);
REGISTER_MSXDEVICE(RomHolyQuran2, "RomHolyQuran2");


RomHolyQuran2::Blocks::Blocks(RomHolyQuran2& device_)
	: RomBlockDebuggableBase(device_)
{
}

byte RomHolyQuran2::Blocks::read(unsigned address)
{
	if ((address < 0x4000) || (address >= 0xc000)) return 255;
	unsigned page = (address - 0x4000) / 0x2000;
	auto& device = OUTER(RomHolyQuran2, romBlocks);
	return (device.bank[page] - &device.rom[0]) / 0x2000;
}

} // namespace openmsx
