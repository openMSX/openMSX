#ifndef XMLLOADER_HH
#define XMLLOADER_HH

#include "XMLElement.hh"

namespace openmsx::XMLLoader {
	XMLElement load(string_view filename, string_view systemID);
}

#endif
