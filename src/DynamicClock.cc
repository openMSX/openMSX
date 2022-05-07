#include "DynamicClock.hh"
#include "serialize.hh"
#include <cassert>

namespace openmsx {

template<Archive Ar>
void DynamicClock::serialize(Ar& ar, unsigned version)
{
	ar.serialize("lastTick", lastTick);
	if (ar.versionAtLeast(version, 2)) {
		EmuDuration period = getPeriod();
		ar.serialize("period", period);
		setPeriod(period);
	} else {
		// Because of possible rounding errors 'auto f = getFreq()'
		// followed by 'setFreq(f)' is not guaranteed to reproduce the
		// exact same result. So in newer versions serialize the period
		// instead of the frequency.
		assert(Ar::IS_LOADER);
		unsigned freq = 0;
		ar.serialize("freq", freq);
		setFreq(freq);
	}
}
INSTANTIATE_SERIALIZE_METHODS(DynamicClock);

} // namespace openmsx
