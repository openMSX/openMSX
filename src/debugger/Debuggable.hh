#ifndef DEBUGGABLE_HH
#define DEBUGGABLE_HH

#include "openmsx.hh"
#include <string_view>

namespace openmsx {

class Debuggable
{
public:
	[[nodiscard]] virtual unsigned getSize() const = 0;
	[[nodiscard]] virtual std::string_view getDescription() const = 0;
	[[nodiscard]] virtual byte read(unsigned address) = 0;
	virtual void write(unsigned address, byte value) = 0;

protected:
	Debuggable() = default;
	~Debuggable() = default;
};

} // namespace openmsx

#endif
