// $Id$

#ifndef __FILEEXCEPTION_HH__
#define __FILEEXCEPTION_HH__

#include "MSXException.hh"

namespace openmsx {

class FileException : public MSXException {
public:
	FileException(const std::string& mes) : MSXException(mes) {}
};

} // namespace openmsx

#endif

