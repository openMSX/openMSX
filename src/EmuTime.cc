// $Id$

#include "EmuTime.hh"
#include <limits>
#include <iostream>

namespace openmsx {

const EmuTime EmuTime::zero((uint64)0);
const EmuTime EmuTime::infinity(std::numeric_limits<uint64>::max());

std::ostream& operator<<(std::ostream &os, const EmuTime &et)
{
	os << et.time;
	return os;
}

} // namespace openmsx
