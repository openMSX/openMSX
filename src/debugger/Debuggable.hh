#ifndef DEBUGGABLE_HH
#define DEBUGGABLE_HH

#include "openmsx.hh"
#include <string>

namespace openmsx {

class Debuggable
{
public:
	virtual unsigned getSize() const = 0;
	virtual const std::string& getDescription() const = 0;
	virtual byte read(unsigned address) = 0;
	virtual void write(unsigned address, byte value) = 0;

protected:
	Debuggable() {}
	~Debuggable() {}
};

} // namespace openmsx

#endif
