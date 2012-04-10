// $Id$

#ifndef FILENOTFOUNDEXCEPTION_HH
#define FILENOTFOUNDEXCEPTION_HH

#include "MSXException.hh"

namespace openmsx {

class FileNotFoundException : public FileException
{
public:
	explicit FileNotFoundException(const std::string& message)
		: FileException(message) {}
	explicit FileNotFoundException(const char*        message)
		: FileException(message) {}
};

} // namespace openmsx

#endif
