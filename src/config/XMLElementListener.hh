// $Id$

#ifndef XMLELEMENTLISTENER_HH
#define XMLELEMENTLISTENER_HH

namespace openmsx {

class XMLElement;

class XMLElementListener
{
public:
	virtual void updateData(const XMLElement& element) = 0;
};

} // namespace openmsx

#endif
