// $Id$

#ifndef XMLEXCEPTION_HH
#define XMLEXCEPTION_HH

#include "MSXException.hh"

namespace openmsx {

class XMLException : public MSXException
{
public:
	explicit XMLException(const std::string& msg)
		: MSXException(msg) {}
};

} // namespace openmsx

#endif
