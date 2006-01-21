// $Id$

#ifndef RAM_HH
#define RAM_HH

#include "openmsx.hh"
#include <cassert>
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class RamDebuggable;

class Ram
{
public:
	Ram(MSXMotherBoard& motherBoard, const std::string& name,
	    const std::string& description, unsigned size);
	virtual ~Ram();

	const byte& operator[](unsigned addr) const {
		assert(addr < size);
		return ram[addr];
	}
	byte& operator[](unsigned addr) {
		assert(addr < size);
		return ram[addr];
	}
	unsigned getSize() const { return size; }
	void clear();

private:
	unsigned size;
	byte* ram;
	const std::auto_ptr<RamDebuggable> debuggable;
};

} // namespace openmsx

#endif
