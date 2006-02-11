// $Id$

#ifndef CHECKEDRAM_HH
#define CHECKEDRAM_HH

#include "CPU.hh"
#include "openmsx.hh"
#include <vector>
#include <bitset>
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class Ram;
class MSXCPU;
class CommandController;

/**
 * This class keeps track of which bytes in the Ram have been written to. It
 * can be used for debugging MSX programs, because you can see if you are
 * trying to read/execute uninitialized memory. Currently all normal RAM
 * (MSXRam) and all normal memory mappers (MSXMemoryMappers) use CheckedRam. On
 * the turboR, only the normal memory mapper runs via CheckedRam. The RAM
 * accessed in DRAM mode or via the ROM mapper are unchecked! Note that there
 * is basically no overhead for using CheckedRam over Ram, thanks to Wouter.
 */
class CheckedRam
{
public:
	CheckedRam(MSXMotherBoard& motherBoard, const std::string& name,
	           const std::string& description, unsigned size);
	virtual ~CheckedRam();

	byte read(unsigned addr);
	byte peek(unsigned addr) const;
	void write(unsigned addr, const byte value);
	
	const byte* getReadCacheLine(unsigned addr) const;
	byte* getWriteCacheLine(unsigned addr) const;

	unsigned getSize() const;
	void clear();

	/**
	 * Give access to the unchecked Ram. No problem to use it, but there
	 * will just be no checking done! Keep in mind that you should use this
	 * consistently, so that the initialized-administration will be always
	 * up to date!
	 */
	Ram& getUncheckedRam() const;


private:
	void callUMRCallBack(unsigned addr);
	
	std::vector<bool> completely_initialized_cacheline;
	std::vector<std::bitset<CPU::CACHE_LINE_SIZE> > uninitialized;
	const std::auto_ptr<Ram> ram;
	MSXCPU& msxcpu;
	CommandController& commandController;
};

} // namespace openmsx

#endif
