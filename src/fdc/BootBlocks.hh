// $Id$

#ifndef BOOTBLOCKS_HH
#define BOOTBLOCKS_HH

namespace openmsx {

class BootBlocks {

public:
	// bootblock created with regular nms8250 and '_format'
	static const byte dos1BootBlock[512];

	// bootblock created with nms8250 and MSX-DOS 2.20
	static const byte dos2BootBlock[512];
	
}; // class BootBlocks

} // namespace openmsx

#endif
