#include "EmuTime.hh"
#include <iostream>

namespace openmsx {

std::ostream& operator<<(std::ostream& os, EmuTime::param time)
{
	os << time.time;
	return os;
}

} // namespace openmsx
