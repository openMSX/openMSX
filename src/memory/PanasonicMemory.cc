#include "PanasonicMemory.hh"
#include "MSXMotherBoard.hh"
#include "MSXCPU.hh"
#include "Ram.hh"
#include "Rom.hh"
#include "DeviceConfig.hh"
#include "HardwareConfig.hh"
#include "XMLElement.hh"
#include "MSXException.hh"
#include <memory>

namespace openmsx {

PanasonicMemory::PanasonicMemory(MSXMotherBoard& motherBoard)
	: msxcpu(motherBoard.getCPU())
	, rom([&]() -> std::optional<Rom> {
		const XMLElement* elem = motherBoard.getMachineConfig()->
				getConfig().findChild("PanasonicRom");
		if (!elem) return std::nullopt;

		const HardwareConfig* hwConf = motherBoard.getMachineConfig();
		assert(hwConf);
		return std::optional<Rom>(std::in_place,
			"PanasonicRom", "Turbor-R main ROM",
			DeviceConfig(*hwConf, *elem));
	}())
{
}

void PanasonicMemory::registerRam(Ram& ram_)
{
	ram = &ram_[0];
	ramSize = ram_.getSize();
}

const byte* PanasonicMemory::getRomBlock(unsigned block)
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
		return ram + ramOffset + offset;
	} else {
		unsigned offset = block * 0x2000;
		if (offset >= rom->getSize()) {
			offset &= rom->getSize() - 1;
		}
		return &(*rom)[offset];
	}
}

const byte* PanasonicMemory::getRomRange(unsigned first, unsigned last)
{
	if (!rom) {
		throw MSXException("Missing PanasonicRom.");
	}
	if (last < first) {
		throw MSXException("Error in config file: firstblock must "
		                   "be smaller than lastblock");
	}
	unsigned start =  first     * 0x2000;
	if (start >= rom->getSize()) {
		throw MSXException("Error in config file: firstblock lies "
		                   "outside of rom image.");
	}
	unsigned stop  = (last + 1) * 0x2000;
	if (stop > rom->getSize()) {
		throw MSXException("Error in config file: lastblock lies "
		                   "outside of rom image.");
	}
	return &(*rom)[start];
}

byte* PanasonicMemory::getRamBlock(unsigned block)
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
