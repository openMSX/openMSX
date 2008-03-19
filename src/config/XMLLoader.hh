// $Id$

#ifndef XMLLOADER_HH
#define XMLLOADER_HH

#include <string>
#include <memory>
#include <libxml/parser.h>

namespace openmsx {

class XMLElement;

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
