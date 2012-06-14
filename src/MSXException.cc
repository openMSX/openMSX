// $Id$

#include "MSXException.hh"

namespace openmsx {

// class MSXException

MSXException::MSXException(string_ref message_)
	: message(message_.data(), message_.size())
{
}

MSXException::~MSXException()
{
}


// class FatalError

FatalError::FatalError(string_ref message_)
	: message(message_.data(), message_.size())
{
}

FatalError::~FatalError()
{
}

} // namespace openmsx
