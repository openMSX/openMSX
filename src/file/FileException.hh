// $Id$

#ifndef FILEEXCEPTION_HH
#define FILEEXCEPTION_HH

#include "MSXException.hh"

namespace openmsx {

class FileException : public MSXException
{
public:
	explicit FileException(const std::string& message)
		: MSXException(message) {}
	explicit FileException(const char*        message)
		: MSXException(message) {}
};

} // namespace openmsx

#endif
