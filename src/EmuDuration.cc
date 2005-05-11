// $Id$

#include "EmuDuration.hh"
#include <limits>

namespace openmsx {

const EmuDuration EmuDuration::zero((uint64)0);
const EmuDuration EmuDuration::infinity(std::numeric_limits<uint64>::max());

} // namespace openmsx
