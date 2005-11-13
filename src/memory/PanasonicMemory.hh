// $Id$

#ifndef PANASONICMEMORY_HH
#define PANASONICMEMORY_HH

#include "openmsx.hh"
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class MSXCPU;
class Ram;
class Rom;

class PanasonicMemory
{
public:
	explicit PanasonicMemory(MSXMotherBoard& motherBoard);
	~PanasonicMemory();

	void registerRam(Ram& ram);
	const byte* getRomBlock(unsigned block);
	const byte* getRomRange(unsigned first, unsigned last);
	byte* getRamBlock(unsigned block);
	void setDRAM(bool dram);

private:
	const std::auto_ptr<Rom> rom;
	byte* ram;
	unsigned ramSize;
	bool dram;

	MSXCPU& msxcpu;
};

} // namespace openmsx

#endif
