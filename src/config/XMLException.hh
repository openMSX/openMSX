#ifndef XMLEXCEPTION_HH
#define XMLEXCEPTION_HH

#include "MSXException.hh"

namespace openmsx {

class XMLException final : public MSXException
{
public:
	explicit XMLException(string_ref message_)
		: MSXException(message_) {}
};

} // namespace openmsx

#endif
