// $Id$

#ifndef FILENOTFOUNDEXCEPTION_HH
#define FILENOTFOUNDEXCEPTION_HH

#include "FileException.hh"

namespace openmsx {

class FileNotFoundException : public FileException
{
public:
	explicit FileNotFoundException(string_ref message)
		: FileException(message) {}
};

} // namespace openmsx

#endif
