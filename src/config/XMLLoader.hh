#ifndef XMLLOADER_HH
#define XMLLOADER_HH

#include "XMLElement.hh"

namespace openmsx {
namespace XMLLoader {

	XMLElement load(const std::string& filename,
	                const std::string& systemID);

} // namespace XMLLoader
} // namespace openmsx

#endif
