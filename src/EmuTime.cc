#include "EmuTime.hh"
#include <iostream>

namespace openmsx {

std::ostream& operator<<(std::ostream& os, EmuTime time)
{
	os << time.time;
	return os;
}

} // namespace openmsx
