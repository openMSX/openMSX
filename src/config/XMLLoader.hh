// $Id$

#ifndef XMLLOADER_HH
#define XMLLOADER_HH

#include "MSXException.hh"
#include <libxml/parser.h>
#include <map>
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
	typedef std::map<std::string, unsigned> IdMap;
	static std::string makeUnique(const std::string& str, IdMap& map);

	static std::auto_ptr<XMLElement> loadXML(const std::string& filename,
	                                         const std::string& systemID,
	                                         IdMap* idMap = NULL);
private:
	static void init(XMLElement& elem, xmlNodePtr node, IdMap* idMap);
};

} // namespace openmsx

#endif
