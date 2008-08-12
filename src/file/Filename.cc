// $Id$

#include "Filename.hh"
#include "FileContext.hh"

using std::string;

namespace openmsx {

Filename::Filename(const string& filename)
	: originalFilename(filename)
	, resolvedFilename(filename)
{
}

Filename::Filename(const string& filename, CommandController& controller)
	: originalFilename(filename)
	, resolvedFilename(UserFileContext().resolve(controller, originalFilename))
{
}

Filename::Filename(const string& filename, const FileContext& context)
	: originalFilename(filename)
	, resolvedFilename(context.resolve(*static_cast<CommandController*>(NULL),
	                                   originalFilename))
{
}

Filename::Filename(const string& filename, const FileContext& context,
                   CommandController& controller)
	: originalFilename(filename)
	, resolvedFilename(context.resolve(controller, originalFilename))
{
}

const string& Filename::getOriginal() const
{
	return originalFilename;
}

const string Filename::getResolved() const
{
	return resolvedFilename;
}

} // namespace openmsx
