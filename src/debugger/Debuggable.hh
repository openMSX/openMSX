#ifndef DEBUGGABLE_HH
#define DEBUGGABLE_HH

#include "openmsx.hh"
#include <string>

namespace openmsx {

class Debuggable
{
public:
	[[nodiscard]] virtual unsigned getSize() const = 0;
	[[nodiscard]] virtual const std::string& getDescription() const = 0;
	[[nodiscard]] virtual byte read(unsigned address) = 0;
	virtual void write(unsigned address, byte value) = 0;

protected:
	Debuggable() = default;
	~Debuggable() = default;
};

} // namespace openmsx

#endif
