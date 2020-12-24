#ifndef XMLLOADER_HH
#define XMLLOADER_HH

#include <string>
#include <string_view>

namespace openmsx {

class XMLElement;

namespace XMLLoader {

[[nodiscard]] XMLElement load(const std::string& filename, std::string_view systemID);

} // namespace XMLLoader
} // namespace openmsx

#endif
