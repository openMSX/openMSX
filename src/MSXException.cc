#include "MSXException.hh"

namespace openmsx {

// class MSXException

MSXException::MSXException(string_ref message_)
	: message(message_.str())
{
}

MSXException::~MSXException()
{
}


// class FatalError

FatalError::FatalError(string_ref message_)
	: message(message_.str())
{
}

FatalError::~FatalError()
{
}

} // namespace openmsx
