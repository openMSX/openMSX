#ifndef XMLLOADER_HH
#define XMLLOADER_HH

#include "XMLElement.hh"

namespace openmsx {
namespace XMLLoader {

	XMLElement load(string_view filename, string_view systemID);

} // namespace XMLLoader
} // namespace openmsx

#endif
