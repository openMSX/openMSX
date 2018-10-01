#ifndef FILENOTFOUNDEXCEPTION_HH
#define FILENOTFOUNDEXCEPTION_HH

#include "FileException.hh"

namespace openmsx {

class FileNotFoundException : public FileException
{
public:
	using FileException::FileException;
};

} // namespace openmsx

#endif
