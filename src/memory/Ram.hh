// $Id$

#ifndef RAM_HH
#define RAM_HH

#include <cassert>
#include "openmsx.hh"
#include "Debuggable.hh"

namespace openmsx {

class Ram : public Debuggable
{
public:
	Ram(const std::string& name, unsigned size);
	Ram(const std::string& name, const std::string& description,
	    unsigned size);
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
	virtual const std::string& getDescription() const;
	virtual byte read(unsigned address);
	virtual void write(unsigned address, byte value);

private:
	void init();
	
	const std::string name;
	const std::string description;
	unsigned size;
	byte* ram;
};

} // namespace openmsx

#endif
