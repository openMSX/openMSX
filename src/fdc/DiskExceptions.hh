// $Id$

#ifndef DISKEXCEPTIONS_HH
#define DISKEXCEPTIONS_HH

#include "MSXException.hh"

namespace openmsx {

class NoSuchSectorException : public MSXException {
public:
	explicit NoSuchSectorException(const std::string& message)
		: MSXException(message) {}
	explicit NoSuchSectorException(const char*        message)
		: MSXException(message) {}
};

class DiskIOErrorException  : public MSXException {
public:
	explicit DiskIOErrorException(const std::string& message)
		: MSXException(message) {}
	explicit DiskIOErrorException(const char*        message)
		: MSXException(message) {}
};

class DriveEmptyException  : public MSXException {
public:
	explicit DriveEmptyException(const std::string& message)
		: MSXException(message) {}
	explicit DriveEmptyException(const char*        message)
		: MSXException(message) {}
};

class WriteProtectedException  : public MSXException {
public:
	explicit WriteProtectedException(const std::string& message)
		: MSXException(message) {}
	explicit WriteProtectedException(const char*        message)
		: MSXException(message) {}
};

} // namespace openmsx

#endif
