#include "EmuTime.hh"
#include "serialize.hh"
#include <limits>
#include <iostream>

namespace openmsx {

const EmuTime EmuTime::zero(uint64_t(0));
const EmuTime EmuTime::infinity(std::numeric_limits<uint64_t>::max());

std::ostream& operator<<(std::ostream& os, EmuTime::param time)
{
	os << time.time;
	return os;
}

template<typename Archive>
void EmuTime::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("time", time);
}
INSTANTIATE_SERIALIZE_METHODS(EmuTime);

} // namespace openmsx
