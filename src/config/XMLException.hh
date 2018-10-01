#ifndef XMLEXCEPTION_HH
#define XMLEXCEPTION_HH

#include "MSXException.hh"

namespace openmsx {

class XMLException final : public MSXException
{
public:
	using MSXException::MSXException;
};

} // namespace openmsx

#endif
