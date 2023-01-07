#ifndef BOOTBLOCKS_HH
#define BOOTBLOCKS_HH

#include "DiskImageUtils.hh"

namespace openmsx {

class BootBlocks
{
public:
	// boot block created with regular nms8250 and '_format'
	static const SectorBuffer dos1BootBlock;

	// boot block created with nms8250 and MSX-DOS 2.20
	static const SectorBuffer dos2BootBlock;

	// FAT12 boot block created with Nextor 2.1.1
	static const SectorBuffer nextorBootBlockFAT12;

	// FAT16 boot block created with Nextor 2.1.1
	static const SectorBuffer nextorBootBlockFAT16;
};

} // namespace openmsx

#endif
