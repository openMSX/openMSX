// $Id$

#ifndef __PANASONICMEMORY_HH__
#define __PANASONICMEMORY_HH__

#include "openmsx.hh"

namespace openmsx {

class MSXCPU;
class Rom;
class Ram;

class PanasonicMemory
{
public:
	static PanasonicMemory& instance();

	void registerRom(const Rom& rom);
	void registerRam(Ram& ram);
	const byte* getRomBlock(int block);
	byte* getRamBlock(int block);
	void setDRAM(bool dram);

private:
	PanasonicMemory();
	~PanasonicMemory();

	const byte* rom;
	int romSize;
	byte* ram;
	int ramSize;
	bool dram;

	MSXCPU& msxcpu;
};

} // namespace openmsx

#endif
