// $Id:$

#include "RomSuperLodeRunner.hh"
#include "MSXMotherBoard.hh"
#include "MSXCPUInterface.hh"
#include "Rom.hh"
#include "serialize.hh"
#include <cassert>

namespace openmsx {

RomSuperLodeRunner::RomSuperLodeRunner(
		MSXMotherBoard& motherBoard, const XMLElement& config,
		std::auto_ptr<Rom> rom)
	: Rom16kBBlocks(motherBoard, config, rom)
{
	getMotherBoard().getCPUInterface().registerGlobalWrite(*this, 0x0000);
	reset(*static_cast<EmuTime*>(0));
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
