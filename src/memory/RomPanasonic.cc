#include "RomPanasonic.hh"
#include "PanasonicMemory.hh"
#include "MSXMotherBoard.hh"
#include "DeviceConfig.hh"
#include "SRAM.hh"
#include "CacheLine.hh"
#include "one_of.hh"
#include "ranges.hh"
#include "serialize.hh"
#include "xrange.hh"
#include <memory>

namespace openmsx {

const int SRAM_BASE = 0x80;
const int RAM_BASE  = 0x180;

RomPanasonic::RomPanasonic(const DeviceConfig& config, Rom&& rom_)
	: Rom8kBBlocks(config, std::move(rom_))
	, panasonicMem(getMotherBoard().getPanasonicMemory())
{
	unsigned sramSize = config.getChildDataAsInt("sramsize", 0);
	if (sramSize) {
		sram = std::make_unique<SRAM>(
			getName() + " SRAM", sramSize * 1024, config);
	}

	if (config.getChildDataAsBool("sram-mirrored", false)) {
		maxSRAMBank = SRAM_BASE + 8;
	} else {
		maxSRAMBank = SRAM_BASE + (sramSize / 8);
	}

	// Inform baseclass about PanasonicMemory (needed for serialization).
	// This relies on the order of MSXDevice instantiation, the PanasonicRam
	// device must be create before this one. (In the hardwareconfig.xml
	// we added a device-reference from this device to the PanasonicRam
	// device, this should protected against wrong future edits in the
	// config file).
	setExtraMemory(panasonicMem.getRamBlock(0), panasonicMem.getRamSize());

	reset(EmuTime::dummy());
}

void RomPanasonic::reset(EmuTime::param /*time*/)
{
	control = 0;
	ranges::fill(bankSelect, 0);
	for (auto region : xrange(8)) {
		setRom(region, 0);
	}
	invalidateDeviceRCache(0x7FF0 & CacheLine::HIGH, CacheLine::SIZE);
}

byte RomPanasonic::peekMem(word address, EmuTime::param time) const
{
	if ((control & 0x04) && (0x7FF0 <= address) && (address < 0x7FF8)) {
		// read mapper state (lower 8 bit)
		return bankSelect[address & 7] & 0xFF;
	} else if ((control & 0x10) && (address == 0x7FF8)) {
		// read mapper state (9th bit)
		byte result = 0;
		for (int i = 7; i >= 0; i--) {
			result <<= 1;
			if (bankSelect[i] & 0x100) {
				result++;
			}
		}
		return result;
	} else if ((control & 0x08) && (address == 0x7FF9)) {
		// read control byte
		return control;
	} else {
		return Rom8kBBlocks::peekMem(address, time);
	}
}

byte RomPanasonic::readMem(word address, EmuTime::param time)
{
	return RomPanasonic::peekMem(address, time);
}

const byte* RomPanasonic::getReadCacheLine(word address) const
{
	if ((0x7FF0 & CacheLine::HIGH) == address) {
		// TODO check mirrored
		return nullptr;
	} else {
		return Rom8kBBlocks::getReadCacheLine(address);
	}
}

void RomPanasonic::writeMem(word address, byte value, EmuTime::param /*time*/)
{
	if ((0x6000 <= address) && (address < 0x7FF0)) {
		// set mapper state (lower 8 bits)
		int region = (address & 0x1C00) >> 10;
		if (region == one_of(5, 6)) region ^= 3;
		int selectedBank = bankSelect[region];
		int newBank = (selectedBank & ~0xFF) | value;
		changeBank(region, newBank);
	} else if (address == 0x7FF8) {
		// set mapper state (9th bit)
		for (auto region : xrange(8)) {
			if (value & 1) {
				changeBank(region, bankSelect[region] |  0x100);
			} else {
				changeBank(region, bankSelect[region] & ~0x100);
			}
			value >>= 1;
		}
	} else if (address == 0x7FF9) {
		// write control byte
		control = value;
	} else {
		// Tested on real FS-A1GT:
		//  SRAM/RAM can be written to from all regions, including e.g.
		//  address 0x7ff0. (I didn't actually test 0xc000-0xffff).
		int region = address >> 13;
		int selectedBank = bankSelect[region];
		if (sram && (SRAM_BASE <= selectedBank) &&
		    (selectedBank < maxSRAMBank)) {
			// SRAM
			int block = selectedBank - SRAM_BASE;
			sram->write((block * 0x2000) | (address & 0x1FFF), value);
		} else if (RAM_BASE <= selectedBank) {
			// RAM
			const_cast<byte*>(bankPtr[region])[address & 0x1FFF] = value;
		}
	}
}

byte* RomPanasonic::getWriteCacheLine(word address) const
{
	if ((0x6000 <= address) && (address < 0x8000)) {
		// mapper select (low/high), control
		return nullptr;
	} else {
		int region = address >> 13;
		int selectedBank = bankSelect[region];
		if (sram && (SRAM_BASE <= selectedBank) &&
		    (selectedBank < maxSRAMBank)) {
			// SRAM
			return nullptr;
		} else if (RAM_BASE <= selectedBank) {
			// RAM
			return const_cast<byte*>(&bankPtr[region][address & 0x1FFF]);
		} else {
			return unmappedWrite;
		}
	}
}

void RomPanasonic::changeBank(byte region, int bank)
{
	if (bank == bankSelect[region]) {
		return;
	}
	bankSelect[region] = bank;

	if (sram && (SRAM_BASE <= bank) && (bank < maxSRAMBank)) {
		// SRAM
		int offset = (bank - SRAM_BASE) * 0x2000;
		int sramSize = sram->getSize();
		if (offset >= sramSize) {
			offset &= (sramSize - 1);
		}
		// TODO romblock debuggable is only 8 bits, here bank is 9 bits
		setBank(region, &sram->operator[](offset), bank);
	} else if (panasonicMem.getRamSize() && (RAM_BASE <= bank)) {
		// RAM
		// TODO romblock debuggable is only 8 bits, here bank is 9 bits
		setBank(region, panasonicMem.getRamBlock(bank - RAM_BASE), bank);
	} else {
		// ROM
		setRom(region, bank);
	}
	invalidateDeviceWCache(0x2000 * region, 0x2000); // 'R' is already handled
	if (region == 3) {
		// don't pre-fill [0x7ff0, 0x7fff]
		invalidateDeviceRCache(0x7FF0 & CacheLine::HIGH, CacheLine::SIZE);
	}
}

template<typename Archive>
void RomPanasonic::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Rom8kBBlocks>(*this);
	ar.serialize("bankSelect", bankSelect,
	             "control",    control);
}
INSTANTIATE_SERIALIZE_METHODS(RomPanasonic);
REGISTER_MSXDEVICE(RomPanasonic, "RomPanasonic");

} // namespace openmsx
