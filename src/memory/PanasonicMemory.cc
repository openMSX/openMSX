// $Id$

#include "PanasonicMemory.hh"
#include "MSXMotherBoard.hh"
#include "MSXCPU.hh"
#include "Ram.hh"
#include "Rom.hh"
#include "HardwareConfig.hh"
#include "MSXException.hh"

namespace openmsx {

static XMLElement& getRomConfig(MSXMotherBoard& motherBoard)
{
	return motherBoard.getHardwareConfig().getChild("PanasonicRom");
}

PanasonicMemory::PanasonicMemory(MSXMotherBoard& motherBoard)
	: rom(new Rom(motherBoard, "PanasonicRom", "Turbo-R main ROM",
	              getRomConfig(motherBoard)))
	, ram(NULL), dram(false)
	, msxcpu(motherBoard.getCPU())
{
}

PanasonicMemory::~PanasonicMemory()
{
}

void PanasonicMemory::registerRam(Ram& ram_)
{
	ram = &ram_[0];
	ramSize = ram_.getSize();
}

const byte* PanasonicMemory::getRomBlock(unsigned block)
{
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
	if (last < first) {
		throw FatalError("Error in config file: firstblock must "
		                 "be smaller than lastblock");
	}
	unsigned start =  first     * 0x2000;
	if (start >= rom->getSize()) {
		throw FatalError("Error in config file: firstblock lies "
		                 "outside of rom image.");
	}
	unsigned stop  = (last + 1) * 0x2000;
	if (stop > rom->getSize()) {
		throw FatalError("Error in config file: lastblock lies "
		                 "outside of rom image.");
	}
	return &(*rom)[start];
}

byte* PanasonicMemory::getRamBlock(unsigned block)
{
	assert(ram);
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
		msxcpu.invalidateMemCache(0x0000, 0x10000);
	}
}

} // namespace openmsx
