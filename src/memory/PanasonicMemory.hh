// $Id$

#ifndef __PANASONICMEMORY_HH__
#define __PANASONICMEMORY_HH__

#include "openmsx.hh"
#include "Rom.hh"

namespace openmsx {

class MSXCPU;
class Ram;

class PanasonicMemory
{
public:
	static PanasonicMemory& instance();

	void registerRam(Ram& ram);
	const byte* getRomBlock(unsigned block);
	byte* getRamBlock(unsigned block);
	void setDRAM(bool dram);

private:
	PanasonicMemory();
	~PanasonicMemory();

	Rom rom;
	byte* ram;
	unsigned ramSize;
	bool dram;

	MSXCPU& msxcpu;
};

} // namespace openmsx

#endif
