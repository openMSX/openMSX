// $Id$

#ifndef CHECKEDRAM_HH
#define CHECKEDRAM_HH

#include "openmsx.hh"
#include "CPU.hh"
#include <vector>
#include <bitset>

namespace openmsx {

class MSXMotherBoard;
class Ram;
class MSXCPU;

/**
 * This class keeps track of which bytes in the Ram have been written to. It
 * can be used for debugging MSX programs, because you can see if you are
 * trying to read/execute uninitialized memory. Currently all normal RAM
 * (MSXRam) and all normal memory mappers (MSXMemoryMappers) use CheckedRam. On
 * the turboR, only the normal memory mapper runs via CheckedRam. The RAM
 * accessed in DRAM mode or via the ROM mapper are unchecked! Note that there
 * is basically no overhead for this CheckedRam, thanks to Wouter.
 */
class CheckedRam
{
public:
	CheckedRam(MSXMotherBoard& motherBoard, const std::string& name,
	    const std::string& description, unsigned size);
	virtual ~CheckedRam();

	const byte read(unsigned addr);
	const byte peek(unsigned addr) const;
	void write(unsigned addr, const byte value);
	
	const byte* getReadCacheLine(unsigned addr);
	byte* getWriteCacheLine(unsigned addr);

	unsigned getSize();
	void clear();

	/**
	 * Give access to the unchecked Ram. No problem to use it, but there
	 * will just be no checking done! Keep in mind that you should use this
	 * consistently, so that the initialized-administration will be always
	 * up to date!
	 */
	Ram* getUncheckedRam();

private:
	unsigned size;
	std::vector<bool> completely_initialized_cacheline;
	std::vector<std::bitset<CPU::CACHE_LINE_SIZE> > uninitialized;
	Ram* ram;
	MSXCPU& msxcpu;

	unsigned umrcount;
};

} // namespace openmsx

#endif
