// $Id$

#ifndef FILEEXCEPTION_HH
#define FILEEXCEPTION_HH

#include "MSXException.hh"

namespace openmsx {

class FileException : public MSXException {
public:
	FileException(const std::string& mes) : MSXException(mes) {}
};

} // namespace openmsx

#endif
