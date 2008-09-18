// $Id$

/*
 * HALNOTE mapper
 *
 * This info is extracted from the romMapperHalnote.c source in blueMSX,
 * implemented by white_cat.
 *
 * This is a 1024kB mapper, it's divided in 128 pages of 8kB. The last 512kB
 * can also be mapped as 256 pages of 2kB. There is 16kB SRAM.
 *
 * Main bankswitch registers:
 *   bank 0,  region: [0x4000-0x5FFF],  switch addr: 0x4FFF
 *   bank 1,  region: [0x6000-0x7FFF],  switch addr: 0x6FFF
 *   bank 2,  region: [0x8000-0x9FFF],  switch addr: 0x8FFF
 *   bank 3,  region: [0xA000-0xBFFF],  switch addr: 0xAFFF
 * Sub-bankswitch registers:
 *   bank 0,  region: [0x7000-0x77FF],  switch addr: 0x77FF
 *   bank 1,  region: [0x7800-0x7FFF],  switch addr: 0x7FFF
 * Note that the two sub-banks overlap with main bank 1!
 *
 * The upper bit (0x80) of the first two main bankswitch registers are special:
 *   bank 0, bit7   SRAM      enabled in [0x0000-0x3FFF]  (1=enabled)
 *   bank 1, bit7   submapper enabled in [0x7000-0x7FFF]  (1=enabled)
 * If enabled, the submapper shadows (part of) main bank 1.
 */

#include "RomHalnote.hh"
#include "CacheLine.hh"
#include "Rom.hh"
#include "SRAM.hh"
#include "serialize.hh"

namespace openmsx {

RomHalnote::RomHalnote(MSXMotherBoard& motherBoard, const XMLElement& config,
                       std::auto_ptr<Rom> rom)
	: Rom8kBBlocks(motherBoard, config, rom)
{
	sram.reset(new SRAM(motherBoard, getName() + " SRAM", 0x4000, config));
	reset(*static_cast<EmuTime*>(0));
}

RomHalnote::~RomHalnote()
{
}

void RomHalnote::reset(const EmuTime& /*time*/)
{
	subBanks[0] = subBanks[1] = 0;
	sramEnabled = false;
	subMapperEnabled = false;

	setBank(0, unmappedRead);
	setBank(1, unmappedRead);
	for (int i = 2; i < 6; i++) {
		setRom(i, 0);
	}
	setBank(6, unmappedRead);
	setBank(7, unmappedRead);
}

const byte* RomHalnote::getReadCacheLine(word address) const
{
	if (subMapperEnabled && (0x7000 <= address) && (address < 0x8000)) {
		// sub-mapper
		int subBank = address < 0x7800 ? 0 : 1;
		return &(*rom)[0x80000 + subBanks[subBank] * 0x800 + (address & 0x7FF)];
	} else {
		// default mapper implementation
		return Rom8kBBlocks::getReadCacheLine(address);
	}
}
byte RomHalnote::readMem(word address, const EmuTime& /*time*/)
{
	// all reads are cacheable, reuse that implementation
	return *getReadCacheLine(address);
}

void RomHalnote::writeMem(word address, byte value, const EmuTime& /*time*/)
{
	if (address < 0x4000) {
		// SRAM region
		if (sramEnabled) {
			sram->write(address, value);
		}
	} else if (address < 0xC000) {
		if ((address == 0x77FF) || (address == 0x7FFF)) {
			// sub-mapper bank switch region
			int subBank = address < 0x7800 ? 0 : 1;
			if (subBanks[subBank] != value) {
				subBanks[subBank] = value;
				if (subMapperEnabled) {
					invalidateMemCache(
						0x7000 + subBank * 0x800, 0x800);
				}
			}
		} else if ((address & 0x1FFF) == 0x0FFF) {
			// normal bank switch region
			int bank = address >> 13; // 2-5
			setRom(bank, value);
			if (bank == 2) {
				// sram enable/disable
				bool newSramEnabled = value & 0x80;
				if (newSramEnabled != sramEnabled) {
					sramEnabled = newSramEnabled;
					if (sramEnabled) {
						setBank(0, &(*sram)[0x0000]);
						setBank(1, &(*sram)[0x2000]);
					} else {
						setBank(0, unmappedRead);
						setBank(1, unmappedRead);
					}
				}
			} else if (bank == 3) {
				// sub-mapper enable/disable
				bool newSubMapperEnabled = value & 0x80;
				if (newSubMapperEnabled != subMapperEnabled) {
					subMapperEnabled = newSubMapperEnabled;
					invalidateMemCache(0x7000, 0x1000);
				}
			}
		}
	}
}

byte* RomHalnote::getWriteCacheLine(word address) const
{
	if (address < 0x4000) {
		// SRAM region
		if (sramEnabled) {
			return NULL;
		}
	} else if (address < 0xC000) {
		if (((address & CacheLine::HIGH) == (0x77FF & CacheLine::HIGH)) ||
		    ((address & CacheLine::HIGH) == (0x7FFF & CacheLine::HIGH))) {
			// sub-mapper bank switch region
			return NULL;
		} else if ((address & 0x1FFF & CacheLine::HIGH) ==
		           (0x0FFF & CacheLine::HIGH)) {
			// normal bank switch region
			return NULL;
		}
	}
	return unmappedWrite;
}

template<typename Archive>
void RomHalnote::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Rom8kBBlocks>(*this);
	ar.serialize("subBanks", subBanks);
	ar.serialize("sramEnabled", sramEnabled);
	ar.serialize("subMapperEnabled", subMapperEnabled);

}
INSTANTIATE_SERIALIZE_METHODS(RomHalnote);
REGISTER_MSXDEVICE(RomHalnote, "RomHalnote");

} // namespace openmsx
