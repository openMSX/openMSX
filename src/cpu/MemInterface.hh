#ifndef MEMINTERFACE_HH
#define MEMINTERFACE_HH

#include "EmuTime.hh"
#include "openmsx.hh"

#include <stdint.h>

namespace openmsx {

class MemInterface
{
public:
	[[nodiscard]] virtual byte peekMem(uint16_t addr, EmuTime::param time) const = 0;
};

} // namespace openmsx

#endif
