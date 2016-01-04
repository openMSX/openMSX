#ifndef FILEEXCEPTION_HH
#define FILEEXCEPTION_HH

#include "MSXException.hh"

namespace openmsx {

class FileException : public MSXException
{
public:
	explicit FileException(string_ref message_)
		: MSXException(message_) {}
};

} // namespace openmsx

#endif
