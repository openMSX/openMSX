#include "RomWonderKid.hh"

// Mapper reverse engineered by Omar Cornut.
// Based on the Wonder Kid game for Game Gear, that is fully MSX compatible.
//
// Only 1 register works and reacts to the whole $8000..BFFF address range
//
// reg $0000 mem $0000..$3fff -- return page 0
// reg $0001 mem $0000..$3fff -- "
//
// reg $4000 mem $4000..$7fff -- return page 0
// reg $7fff mem $4000..$7fff -- "
//
// reg $7fff mem $8000..$bfff --
// reg $8000 mem $8000..$bfff ok
// reg $8001 mem $8000..$bfff ok
// reg $BFFF mem $8000..$bfff ok
// reg $C000 mem $8000..$bfff --
//
//
// Restating more simply: Actual Wonder Kid mapper only has a single register
// which is mirrored across all of 0x8000-0xbfff, and writing to any of the
// mirrors remaps all of 0x8000-0xbfff. Writing in 0x0000-0x7fff, and
// 0xc000-... has no effect.
// 0x0000-0x3fff always shows page 0, as does 0x4000-0x7fff
// The mapper does not respond to reads in 0xc000-0xffff

namespace openmsx {

RomWonderKid::RomWonderKid(const DeviceConfig& config, Rom&& rom_)
	: Rom16kBBlocks(config, std::move(rom_))
{
	reset(EmuTime::dummy());
}

void RomWonderKid::reset(EmuTime /*time*/)
{
	setRom(0, 0); // fixed
	setRom(1, 0); // fixed
	setRom(2, 2); // actually confirmed by Omar
	setUnmapped(3);
}

byte RomWonderKid::peekMem(uint16_t address, EmuTime time) const
{
	if (address < 0xC000) {
		return Rom16kBBlocks::peekMem(address, time);
	}
	return 0xff;
}

byte RomWonderKid::readMem(uint16_t address, EmuTime time)
{
	return peekMem(address, time);
}

void RomWonderKid::writeMem(uint16_t address, byte value, EmuTime /*time*/)
{
	if ((0x8000 <= address) && (address < 0xC000)) {
		setRom(2, value);
	}
}

byte* RomWonderKid::getWriteCacheLine(uint16_t address)
{
	if ((0x8000 <= address) && (address < 0xC000)) {
		return nullptr;
	} else {
		return unmappedWrite.data();
	}
}

REGISTER_MSXDEVICE(RomWonderKid, "RomWonderKid");

} // namespace openmsx
