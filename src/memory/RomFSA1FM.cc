// FS-A1FM mappers
//   based on info from:
//   http://sourceforge.net/tracker/index.php?func=detail&aid=672508&group_id=38274&atid=421864
//
// Slot 3-1
//   0x4000-0x5FFF  16 x 8kB ROM, same 16 pages as mapper in slot 3-3
//   0x6000-0x7FFF  8kB RAM   (mapped ???)
//   0x7FC0-0x7FCF  I/O area
//     0x7FC0 (R/W)   8251 (UART) data register
//     0x7FC1 (R/W)   8251 (UART) command/status register
//     0x7FC4 (?)     ROM switch address, lower 4 bits -> selected bank
//                                        upper 4 bits -> ??? (always preserved)
//     0x7FC5 (W)     ???
//     0x7FC6 (R)     ???
//     0x7FC8 (W)     ???
//     0x7FCC (W)     ???
//
// Slot 3-3
//   Very similar to other Panasonic-8kB rom mappers
//
//   0x0000-0x1FFF  switch address 0x6400, read-back 0x7FF0, initial value 0xA8
//   0x2000-0x3FFF  switch address 0x6C00, read-back 0x7FF1, initial value 0xA8
//   0x4000-0x5FFF  switch address 0x6000, read-back 0x7FF2, initial value 0xA8
//   0x6000-0x7FFF  switch address 0x6800, read-back 0x7FF3, initial value 0xA8
//   0x8000-0x9FFF  switch address 0x7000, read-back 0x7FF4, initial value 0xA8
//   0xA000-0xBFFF  switch address 0x7800, read-back 0x7FF5, initial value 0xA8
//
//   0x7FF6-0x7FF7  always read 0
//   0x7FF9         control byte, bit2 activates 0x7FF0-0x7FF7
//
//   mapper pages 0x10-0x7F            mirrored at 0x90-0xFF
//                0x80-0x83 0x88-0x8B  unmapped (read 0xFF)
//                0x84-0x87 0x8C-0x8F  contain (same) 8kB RAM

#include "RomFSA1FM.hh"

#include "SRAM.hh"

#include "CacheLine.hh"
#include "MSXException.hh"
#include "MSXMotherBoard.hh"
#include "serialize.hh"

#include "one_of.hh"
#include "strCat.hh"
#include "xrange.hh"

#include <memory>

namespace openmsx {

// 8kb shared sram //

[[nodiscard]] static std::shared_ptr<SRAM> getSram(const DeviceConfig& config)
{
	return config.getMotherBoard().getSharedStuff<SRAM>(
		"FSA1FM-sram",
		strCat(config.getAttributeValue("id"), " SRAM"), 0x2000, config);
}


// Mapper for slot 3-1 //

RomFSA1FM1::RomFSA1FM1(const DeviceConfig& config, Rom&& rom_)
	: MSXRom(config, std::move(rom_))
	, fsSram(getSram(config))
	, firmwareSwitch(config)
{
	if (rom.size() != one_of(0x100000u, 0x200000u)) {
		throw MSXException(
			"Rom for FSA1FM mapper must be 1MB in size "
			"(some dumps are 2MB, those can be used as well).");
	}
}

void RomFSA1FM1::reset(EmuTime /*time*/)
{
	// initial rom bank is undefined
}

byte RomFSA1FM1::peekMem(uint16_t address, EmuTime /*time*/) const
{
	if ((0x4000 <= address) && (address < 0x6000)) {
		// read rom
		return rom[(0x2000 * ((*fsSram)[0x1FC4] & 0x0F)) +
		              (address & 0x1FFF)];
	} else if ((0x7FC0 <= address) && (address < 0x7FD0)) {
		switch (address & 0x0F) {
		case 4:
			return (*fsSram)[address & 0x1FFF];
		case 6:
			return firmwareSwitch.getStatus() ? 0xFB : 0xFF;
		default:
			return 0xFF;
		}
	} else if ((0x6000 <= address) && (address < 0x8000)) {
		// read sram
		// TODO are there multiple sram blocks?
		return (*fsSram)[address & 0x1FFF];
	} else {
		return 0xFF;
	}
}

byte RomFSA1FM1::readMem(uint16_t address, EmuTime time)
{
	return RomFSA1FM1::peekMem(address, time);
}

const byte* RomFSA1FM1::getReadCacheLine(uint16_t address) const
{
	if (address == (0x7FC0 & CacheLine::HIGH)) {
		// don't cache IO area
		return nullptr;
	} else if ((0x4000 <= address) && (address < 0x6000)) {
		// read rom
		return &rom[(0x2000 * ((*fsSram)[0x1FC4] & 0x0F)) +
		               (address & 0x1FFF)];
	} else if ((0x6000 <= address) && (address < 0x8000)) {
		// read sram
		return &(*fsSram)[address & 0x1FFF];
	} else {
		return unmappedRead.data();
	}
}

void RomFSA1FM1::writeMem(uint16_t address, byte value, EmuTime /*time*/)
{
	// TODO 0x7FC0 - 0x7FCF is modem IO area

	if ((0x6000 <= address) && (address < 0x8000)) {
		if (address == 0x7FC4) {
			// switch rom bank
			invalidateDeviceRCache(0x4000, 0x2000);
		}
		fsSram->write(address & 0x1FFF, value);
	}
}

byte* RomFSA1FM1::getWriteCacheLine(uint16_t address)
{
	if (address == (0x7FC0 & CacheLine::HIGH)) {
		// dont't cache IO area
		return nullptr;
	} else if ((0x6000 <= address) && (address < 0x8000)) {
		// don't cache SRAM writes
		return nullptr;
	} else {
		return unmappedWrite.data();
	}
}

template<typename Archive>
void RomFSA1FM1::serialize(Archive& ar, unsigned /*version*/)
{
	// skip MSXRom base class
	ar.template serializeBase<MSXDevice>(*this);
	// don't serialize (shared) sram here, rely on RomFSA1FM2 to do that
}
INSTANTIATE_SERIALIZE_METHODS(RomFSA1FM1);
REGISTER_MSXDEVICE(RomFSA1FM1, "RomFSA1FM1");


// Mapper for slot 3-3 //

RomFSA1FM2::RomFSA1FM2(const DeviceConfig& config, Rom&& rom_)
	: Rom8kBBlocks(config, std::move(rom_))
	, fsSram(getSram(config))
{
	reset(EmuTime::dummy());
}

void RomFSA1FM2::reset(EmuTime /*time*/)
{
	control = 0;
	for (auto region : xrange(6)) {
		changeBank(region, 0xA8);
	}
	changeBank(6, 0); // for mapper-state read-back
	changeBank(7, 0);
	setUnmapped(6);
	setUnmapped(7);
}

byte RomFSA1FM2::peekMem(uint16_t address, EmuTime time) const
{
	if (0xC000 <= address) {
		return 0xFF;
	} else if ((control & 0x04) && (0x7FF0 <= address) && (address < 0x7FF8)) {
		// read mapper state
		return bankSelect[address & 7];
	} else if (isRam[address >> 13]) {
		return (*fsSram)[address & 0x1FFF];
	} else if (isEmpty[address >> 13]) {
		return 0xFF;
	} else {
		return Rom8kBBlocks::peekMem(address, time);
	}
}

byte RomFSA1FM2::readMem(uint16_t address, EmuTime time)
{
	return RomFSA1FM2::peekMem(address, time);
}

const byte* RomFSA1FM2::getReadCacheLine(uint16_t address) const
{
	if (0xC000 <= address) {
		return unmappedRead.data();
	} else if ((0x7FF0 & CacheLine::HIGH) == address) {
		return nullptr;
	} else if (isRam[address >> 13]) {
		return &(*fsSram)[address & 0x1FFF];
	} else if (isEmpty[address >> 13]) {
		return unmappedRead.data();
	} else {
		return Rom8kBBlocks::getReadCacheLine(address);
	}
}

void RomFSA1FM2::writeMem(uint16_t address, byte value,
                          EmuTime /*time*/)
{
	if ((0x6000 <= address) && (address < 0x7FF0)) {
		// set mapper state
		switch (address & 0x1C00) {
		case 0x0000:
			changeBank(2, value);
			break;
		case 0x0400:
			changeBank(0, value);
			break;
		case 0x0800:
			changeBank(3, value);
			break;
		case 0x0C00:
			changeBank(1, value);
			break;
		case 0x1000:
			changeBank(4, value);
			break;
		case 0x1400:
			// nothing
			break;
		case 0x1800:
			changeBank(5, value);
			break;
		case 0x1C00:
			// nothing
			break;
		}
	} else if (address == 0x7FF9) {
		// write control byte
		control = value;
	} else if (isRam[address >> 13]) {
		fsSram->write(address & 0x1FFF, value);
	}
}

byte* RomFSA1FM2::getWriteCacheLine(uint16_t address)
{
	if ((0x6000 <= address) && (address < 0x8000)) {
		return nullptr;
	} else if (isRam[address >> 13]) {
		return nullptr;
	} else {
		return unmappedWrite.data();
	}
}

void RomFSA1FM2::changeBank(unsigned region, byte bank)
{
	bankSelect[region] = bank;
	if ((0x80 <= bank) && (bank < 0x90)) {
		if (bank & 0x04) {
			isRam[region]   = true;
			isEmpty[region] = false;
		} else {
			isRam[region]   = false;
			isEmpty[region] = true;
		}
		invalidateDeviceRWCache(0x2000 * region, 0x2000);
	} else {
		isRam[region]   = false;
		isEmpty[region] = false;
		setRom(region, bank & 0x7F);
		if (region == 3) {
			invalidateDeviceRCache(0x7FF0 & CacheLine::HIGH, CacheLine::SIZE);
		}
	}
}

template<typename Archive>
void RomFSA1FM2::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Rom8kBBlocks>(*this);
	// note: SRAM can be serialized in this class (as opposed to
	//       Rom8kBBlocks), because we don't use setBank to map it
	ar.serialize("SRAM",       *fsSram,
	             "bankSelect", bankSelect,
	             "control",    control);
	if constexpr (Archive::IS_LOADER) {
		// recalculate 'isRam' and 'isEmpty' from bankSelect
		for (auto region : xrange(8)) {
			changeBank(region, bankSelect[region]);
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(RomFSA1FM2);
REGISTER_MSXDEVICE(RomFSA1FM2, "RomFSA1FM2");

} // namespace openmsx
