#ifndef FILEEXCEPTION_HH
#define FILEEXCEPTION_HH

#include "MSXException.hh"

namespace openmsx {

class FileException : public MSXException
{
public:
	using MSXException::MSXException;
};

} // namespace openmsx

#endif
