// $Id$

#include "MSXException.hh"

namespace openmsx {

// class MSXException

MSXException::MSXException(const std::string& message_)
	: message(message_)
{
}

MSXException::MSXException(const char* message_)
	: message(message_)
{
}

MSXException::~MSXException()
{
}


// class FatalError

FatalError::FatalError(const std::string& message_)
	: message(message_)
{
}

FatalError::FatalError(const char* message_)
	: message(message_)
{
}

FatalError::~FatalError()
{
}

} // namespace openmsx
