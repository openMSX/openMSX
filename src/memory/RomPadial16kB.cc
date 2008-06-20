// $Id$

// Padial 16kB
//
// The address to change banks:
//  first  16kb: 0x6000 - 0x67ff (0x6000 used)
//  second 16kb: 0x7000 - 0x77ff (0x7000 and 0x77ff used)

#include "RomPadial16kB.hh"
#include "Rom.hh"

namespace openmsx {

RomPadial16kB::RomPadial16kB(
		MSXMotherBoard& motherBoard, const XMLElement& config,
		std::auto_ptr<Rom> rom)
	: RomAscii16kB(motherBoard, config, rom)
{
	reset(*static_cast<EmuTime*>(0));
}

void RomPadial16kB::reset(const EmuTime& /*time*/)
{
	setRom (0, 0);
	setRom (1, 0);
	setRom (2, 2);
	setBank(3, unmappedRead);
}

} // namespace openmsx
