#include "EmuDuration.hh"
#include "serialize.hh"
#include <limits>

namespace openmsx {

const EmuDuration EmuDuration::zero(uint64_t(0));
const EmuDuration EmuDuration::infinity(std::numeric_limits<uint64_t>::max());

template<typename Archive>
void EmuDuration::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("time", time);
}
INSTANTIATE_SERIALIZE_METHODS(EmuDuration);

} // namespace openmsx
