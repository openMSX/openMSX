// ASCII-X 16kB cartridges
//
// Banks:
//  first  16kB: 0x4000 - 0x7FFF and 0xC000 - 0xFFFF
//  second 16kB: 0x8000 - 0xBFFF and 0x0000 - 0x3FFF
//
// Address to change banks:
//  first  16kB: 0x6000 - 0x6FFF, 0xA000 - 0xAFFF, 0xE000 - 0xEFFF, 0x2000 - 0x2FFF
//  second 16kB: 0x7000 - 0x7FFF, 0xB000 - 0xBFFF, 0xF000 - 0xFFFF, 0x3000 - 0x3FFF
//
// 12-bit bank number is composed by address bits 8-11 and the data bits.
//
// Backed by FlashROM.

#include "RomAscii16X.hh"
#include "narrow.hh"
#include "outer.hh"
#include "serialize.hh"

namespace openmsx {

RomAscii16X::RomAscii16X(DeviceConfig& config, Rom&& rom_)
	: MSXRom(config, std::move(rom_))
	, debuggable(*this)
	, flash(rom, AmdFlashChip::S29GL064S70TFI040, {}, config)
{
}

void RomAscii16X::reset(EmuTime::param /* time */)
{
	ranges::iota(bankRegs, word(0));

	flash.reset();

	invalidateDeviceRCache(); // flush all to be sure
}

unsigned RomAscii16X::getFlashAddr(word addr) const
{
	word bank = bankRegs[((addr >> 14) & 1) ^ 1];
	return (bank << 14) | (addr & 0x3FFF);
}

byte RomAscii16X::readMem(word addr, EmuTime::param /* time */)
{
	return flash.read(getFlashAddr(addr));
}

byte RomAscii16X::peekMem(word addr, EmuTime::param /* time */) const
{
	return flash.peek(getFlashAddr(addr));
}

const byte* RomAscii16X::getReadCacheLine(word addr) const
{
	return flash.getReadCacheLine(getFlashAddr(addr));
}

void RomAscii16X::writeMem(word addr, byte value, EmuTime::param /* time */)
{
	flash.write(getFlashAddr(addr), value);

	if ((addr & 0x3FFF) >= 0x2000) {
		const word index = (addr >> 12) & 1;
		bankRegs[index] = (addr & 0x0F00) | value;
		invalidateDeviceRCache(0x4000 ^ (index << 14), 0x4000);
		invalidateDeviceRCache(0xC000 ^ (index << 14), 0x4000);
	}
}

byte* RomAscii16X::getWriteCacheLine(word /* addr */)
{
	return nullptr; // not cacheable
}

unsigned RomAscii16X::Debuggable::readExt(unsigned address)
{
	auto& outer = OUTER(RomAscii16X, debuggable);
	return outer.bankRegs[((address >> 14) & 1) ^ 1];
}

template<typename Archive>
void RomAscii16X::serialize(Archive& ar, unsigned /*version*/)
{
	// skip MSXRom base class
	ar.template serializeBase<MSXDevice>(*this);

	ar.serialize("flash",       flash,
	             "bankRegs",    bankRegs);
}
INSTANTIATE_SERIALIZE_METHODS(RomAscii16X);
REGISTER_MSXDEVICE(RomAscii16X, "RomAscii16X");

} // namespace openmsx
