// $Id$

#ifndef DISKEXCEPTIONS_HH
#define DISKEXCEPTIONS_HH

#include "MSXException.hh"

namespace openmsx {

class NoSuchSectorException : public MSXException {
public:
	explicit NoSuchSectorException(const std::string& desc)
		: MSXException(desc) {}
};

class DiskIOErrorException  : public MSXException {
public:
	explicit DiskIOErrorException(const std::string& desc)
		: MSXException(desc) {}
};

class DriveEmptyException  : public MSXException {
public:
	explicit DriveEmptyException(const std::string& desc)
		: MSXException(desc) {}
};

class WriteProtectedException  : public MSXException {
public:
	explicit WriteProtectedException(const std::string& desc)
		: MSXException(desc) {}
};

} // namespace openmsx

#endif
