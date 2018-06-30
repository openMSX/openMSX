#include "Filename.hh"
#include "FileContext.hh"
#include "FileOperations.hh"
#include "MSXException.hh"
#include "serialize.hh"
#include <cassert>

using std::string;

namespace openmsx {

Filename::Filename(string filename)
	: originalFilename(std::move(filename))
	, resolvedFilename(originalFilename)
{
}

Filename::Filename(string filename, const FileContext& context)
	: originalFilename(std::move(filename))
	, resolvedFilename(FileOperations::getAbsolutePath(
		context.resolve(originalFilename)))
{
}

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
	ar.serialize("original", originalFilename);
	ar.serialize("resolved", resolvedFilename);
}
INSTANTIATE_SERIALIZE_METHODS(Filename);

} // namespace openmsx
