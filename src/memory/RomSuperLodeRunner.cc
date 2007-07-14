// $Id:$

#include "RomSuperLodeRunner.hh"
#include "MSXMotherBoard.hh"
#include "MSXCPUInterface.hh"
#include "Rom.hh"
#include <cassert>

namespace openmsx {

RomSuperLodeRunner::RomSuperLodeRunner(
	MSXMotherBoard& motherBoard, const XMLElement& config,
	const EmuTime& time, std::auto_ptr<Rom> rom)
	: Rom16kBBlocks(motherBoard, config, time, rom)
{
	getMotherBoard().getCPUInterface().registerGlobalWrite(*this, 0x0000);
	reset(time);
}

RomSuperLodeRunner::~RomSuperLodeRunner()
{
	getMotherBoard().getCPUInterface().unregisterGlobalWrite(*this, 0x0000);
}

void RomSuperLodeRunner::reset(const EmuTime& /*time*/)
{
	setBank(0, unmappedRead);
	setBank(1, unmappedRead);
	setRom (2, 0);
	setBank(3, unmappedRead);
}

void RomSuperLodeRunner::globalWrite(word address, byte value, const EmuTime& /*time*/)
{
	assert(address == 0);
	(void)address;
	setRom(2, value);
}

} // namespace openmsx
