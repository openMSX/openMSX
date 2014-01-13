#ifndef XMLLOADER_HH
#define XMLLOADER_HH

#include "XMLElement.hh"

namespace openmsx {
namespace XMLLoader {

	XMLElement load(string_ref filename, string_ref systemID);

} // namespace XMLLoader
} // namespace openmsx

#endif
