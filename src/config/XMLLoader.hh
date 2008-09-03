// $Id$

#ifndef XMLLOADER_HH
#define XMLLOADER_HH

#include <string>
#include <memory>

namespace openmsx {

class XMLElement;

namespace XMLLoader
{
	std::auto_ptr<XMLElement> load(const std::string& filename,
	                               const std::string& systemID);
};

} // namespace openmsx

#endif
