// $Id$

#ifndef __RAM_HH__
#define __RAM_HH__

#include <cassert>
#include "openmsx.hh"
#include "Debuggable.hh"

namespace openmsx {

class Ram : public Debuggable
{
public:
	Ram(const string& name, unsigned size);
	Ram(const string& name, const string& description, unsigned size);
	virtual ~Ram();

	const byte& operator[](unsigned addr) const {
		assert(addr < size);
		return ram[addr];
	}
	byte& operator[](unsigned addr) {
		assert(addr < size);
		return ram[addr];
	}
	void clear();

	// Debuggable
	virtual unsigned getSize() const;
	virtual const string& getDescription() const;
	virtual byte read(unsigned address);
	virtual void write(unsigned address, byte value);

private:
	void init();
	
	const string name;
	const string description;
	unsigned size;
	byte* ram;
};

} // namespace openmsx

#endif
