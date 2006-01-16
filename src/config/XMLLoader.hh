// $Id$

#ifndef XMLLOADER_HH
#define XMLLOADER_HH

#include "MSXException.hh"
#include <libxml/parser.h>
#include <memory>

namespace openmsx {

class XMLElement;

class XMLException: public MSXException
{
public:
	explicit XMLException(const std::string& msg);
};

class XMLLoader
{
public:
	static std::auto_ptr<XMLElement> loadXML(const std::string& filename,
	                                         const std::string& systemID);
private:
	static void init(XMLElement& elem, xmlNodePtr node);
};

} // namespace openmsx

#endif
