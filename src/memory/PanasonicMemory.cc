#include "PanasonicMemory.hh"
#include "MSXMotherBoard.hh"
#include "MSXCPU.hh"
#include "Ram.hh"
#include "Rom.hh"
#include "DeviceConfig.hh"
#include "HardwareConfig.hh"
#include "XMLElement.hh"
#include "MSXException.hh"
#include "narrow.hh"
#include <memory>

namespace openmsx {

PanasonicMemory::PanasonicMemory(MSXMotherBoard& motherBoard)
	: msxcpu(motherBoard.getCPU())
	, rom([&] -> std::optional<Rom> {
		auto* elem = motherBoard.getMachineConfig()->
				getConfig().findChild("PanasonicRom");
		if (!elem) return std::nullopt;

		HardwareConfig* hwConf = motherBoard.getMachineConfig();
		assert(hwConf);
		DeviceConfig config(*hwConf, *elem);
		return std::optional<Rom>(std::in_place,
			"PanasonicRom", "Turbor-R main ROM", config);
	}())
{
}

void PanasonicMemory::registerRam(Ram& ram_)
{
	ram = ram_.data();
	ramSize = narrow<unsigned>(ram_.size());
}

std::span<const uint8_t, 0x2000> PanasonicMemory::getRomBlock(unsigned block) const
{
	if (!rom) {
		throw MSXException("Missing PanasonicRom.");
	}
	if (dram &&
	    (((0x28 <= block) && (block < 0x2C)) ||
	     ((0x38 <= block) && (block < 0x3C)))) {
		assert(ram);
		unsigned offset = (block & 0x03) * 0x2000;
		unsigned ramOffset = (block < 0x30) ? ramSize - 0x10000 :
		                                      ramSize - 0x08000;
		return std::span<const uint8_t, 0x2000>{ram + ramOffset + offset, 0x2000};
	} else {
		unsigned offset = block * 0x2000;
		if (offset >= rom->size()) {
			offset &= narrow<unsigned>(rom->size() - 1);
		}
		return subspan<0x2000>(*rom, offset);
	}
}

std::span<const uint8_t> PanasonicMemory::getRomRange(unsigned first, unsigned last) const
{
	if (!rom) {
		throw MSXException("Missing PanasonicRom.");
	}
	if (last < first) {
		throw MSXException("Error in config file: firstblock must "
		                   "be smaller than lastblock");
	}
	unsigned start =  first     * 0x2000;
	if (start >= rom->size()) {
		throw MSXException("Error in config file: firstblock lies "
		                   "outside of rom image.");
	}
	unsigned stop  = (last + 1) * 0x2000;
	if (stop > rom->size()) {
		throw MSXException("Error in config file: lastblock lies "
		                   "outside of rom image.");
	}
	return subspan(*rom, start, stop - start);
}

uint8_t* PanasonicMemory::getRamBlock(unsigned block)
{
	if (!ram) return nullptr;

	unsigned offset = block * 0x2000;
	if (offset >= ramSize) {
		offset &= ramSize - 1;
	}
	return ram + offset;
}

void PanasonicMemory::setDRAM(bool dram_)
{
	if (dram_ != dram) {
		dram = dram_;
		msxcpu.invalidateAllSlotsRWCache(0x0000, 0x10000);
	}
}

bool PanasonicMemory::isWritable(unsigned address) const
{
	return !dram || (address < (ramSize - 0x10000));
}

} // namespace openmsx
