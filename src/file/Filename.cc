// $Id$

#include "Filename.hh"
#include "FileContext.hh"
#include "FileOperations.hh"
#include "serialize.hh"
#include <cassert>

using std::string;

namespace openmsx {

// dummy constructor, to be able to serialize vector<Filename>
Filename::Filename()
{
}

Filename::Filename(const string& filename)
	: originalFilename(filename)
	, resolvedFilename(filename)
{
}

Filename::Filename(const string& filename, CommandController& controller)
	: originalFilename(filename)
	, resolvedFilename(FileOperations::getAbsolutePath(
		UserFileContext().resolve(controller, originalFilename)))
{
}

Filename::Filename(const string& filename, const FileContext& context)
	: originalFilename(filename)
	, resolvedFilename(FileOperations::getAbsolutePath(
		context.resolve(*static_cast<CommandController*>(NULL),
		                originalFilename)))
{
}

Filename::Filename(const string& filename, const FileContext& context,
                   CommandController& controller)
	: originalFilename(filename)
	, resolvedFilename(FileOperations::getAbsolutePath(
		context.resolve(controller, originalFilename)))
{
}

const string& Filename::getOriginal() const
{
	return originalFilename;
}

const string& Filename::getResolved() const
{
	return resolvedFilename;
}

const string& Filename::getAfterLoadState() const
{
	if (FileOperations::exists(resolvedFilename)) {
		return resolvedFilename;
	} else {
		return originalFilename;
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
