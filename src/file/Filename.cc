#include "Filename.hh"
#include "MSXException.hh"
#include "serialize.hh"
#include <cassert>

namespace openmsx {

void Filename::updateAfterLoadState()
{
	if (empty()) return;
	if (FileOperations::exists(resolvedFilename)) return;

	try {
		resolvedFilename = FileOperations::getAbsolutePath(
			userFileContext().resolve(originalFilename));
	} catch (MSXException&) {
		// nothing
	}
}

bool Filename::empty() const
{
	assert(getOriginal().empty() == getResolved().empty());
	return getOriginal().empty();
}

template<typename Archive>
void Filename::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("original", originalFilename,
	             "resolved", resolvedFilename);
}
INSTANTIATE_SERIALIZE_METHODS(Filename);

} // namespace openmsx
