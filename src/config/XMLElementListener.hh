// $Id$

#ifndef XMLELEMENTLISTENER_HH
#define XMLELEMENTLISTENER_HH

namespace openmsx {

class XMLElement;

class XMLElementListener
{
public:
	virtual void updateData(const XMLElement& element) {}
	virtual void childAdded(const XMLElement& parent,
	                        const XMLElement& child) {}
protected:
	virtual ~XMLElementListener() {}
};

} // namespace openmsx

#endif
