#include "DynamicClock.hh"
#include "serialize.hh"

namespace openmsx {

template<typename Archive>
void DynamicClock::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("lastTick", lastTick);
	unsigned freq = getFreq();
	ar.serialize("freq", freq);
	if (ar.isLoader()) setFreq(freq);
}
INSTANTIATE_SERIALIZE_METHODS(DynamicClock);

} // namespace openmsx
