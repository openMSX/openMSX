// $Id$

#include "EmuDuration.hh"
#include "serialize.hh"
#include <limits>

namespace openmsx {

const EmuDuration EmuDuration::zero(uint64(0));
const EmuDuration EmuDuration::infinity(std::numeric_limits<uint64>::max());

template<typename Archive>
void EmuDuration::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("time", time);
}
INSTANTIATE_SERIALIZE_METHODS(EmuDuration);

} // namespace openmsx
