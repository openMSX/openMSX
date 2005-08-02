// $Id$

#ifndef RAM_HH
#define RAM_HH

#include "SimpleDebuggable.hh"
#include "openmsx.hh"
#include <cassert>

namespace openmsx {

class MSXMotherBoard;

class Ram : private SimpleDebuggable
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
	// SimpleDebuggable
	virtual byte read(unsigned address);
	virtual void write(unsigned address, byte value);

	unsigned size;
	byte* ram;
};

} // namespace openmsx

#endif
