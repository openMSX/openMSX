// $Id$

#ifndef __DEBUGGABLE_HH__
#define __DEBUGGABLE_HH__

#include <string>
#include "openmsx.hh"

using std::string;

namespace openmsx {

class Debuggable
{
public:
	virtual unsigned getSize() const = 0;
	virtual const string& getDescription() const = 0;
	virtual byte read(unsigned address) = 0;
	virtual void write(unsigned address, byte value) = 0;

protected:
	Debuggable() {}
	virtual ~Debuggable() {}
};

} // namespace openmsx

#endif
