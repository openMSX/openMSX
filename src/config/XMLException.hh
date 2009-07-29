// $Id$

#ifndef XMLEXCEPTION_HH
#define XMLEXCEPTION_HH

#include "MSXException.hh"

namespace openmsx {

class XMLException : public MSXException
{
public:
	explicit XMLException(const std::string& message)
		: MSXException(message) {}
	explicit XMLException(const char*        message)
		: MSXException(message) {}
};

} // namespace openmsx

#endif
