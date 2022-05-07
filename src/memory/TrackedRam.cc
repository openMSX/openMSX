#include "TrackedRam.hh"
#include "serialize.hh"

namespace openmsx {

void TrackedRam::serialize(Archive auto& ar, unsigned /*version*/)
{
	// Note: This is the exact same serialization format as the Ram class.
	//  This allows to change from Ram to TrackedRam without having to
	//  increase the class serialization version (of the user).
	bool diff = writeSinceLastReverseSnapshot || !ar.isReverseSnapshot();
	ar.serialize_blob("ram", &ram[0], getSize(), diff);
	if (ar.isReverseSnapshot()) writeSinceLastReverseSnapshot = false;
}
INSTANTIATE_SERIALIZE_METHODS(TrackedRam);

} // namespace openmsx
