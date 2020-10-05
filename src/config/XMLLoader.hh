#ifndef XMLLOADER_HH
#define XMLLOADER_HH

#include <string_view>

namespace openmsx {

class XMLElement;

namespace XMLLoader {

XMLElement load(std::string_view filename, std::string_view systemID);

} // namespace XMLLoader
} // namespace openmsx

#endif
