#ifndef BOOTBLOCKS_HH
#define BOOTBLOCKS_HH

#include "DiskImageUtils.hh"

namespace openmsx {

class BootBlocks
{
public:
	// bootblock created with regular nms8250 and '_format'
	static const SectorBuffer dos1BootBlock;

	// bootblock created with nms8250 and MSX-DOS 2.20
	static const SectorBuffer dos2BootBlock;
};

} // namespace openmsx

#endif
