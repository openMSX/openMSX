#include "TrackedRam.hh"
#include "serialize.hh"

namespace openmsx {

template<typename Archive>
void TrackedRam::serialize(Archive& ar, unsigned /*version*/)
{
	// Note: This is the exact same serialization format as the Ram class.
	//  This allows to change from Ram to TrackedRam without having to
	//  increase the class serialization version (of the user).
	bool diff = writeSinceLastReverseSnapshot || !ar.isReverseSnapshot();
	ar.serialize_blob("ram", std::span{ram}, diff);
	if (ar.isReverseSnapshot()) writeSinceLastReverseSnapshot = false;
}
INSTANTIATE_SERIALIZE_METHODS(TrackedRam);

} // namespace openmsx
