#ifndef DISKEXCEPTIONS_HH
#define DISKEXCEPTIONS_HH

#include "MSXException.hh"

namespace openmsx {

class NoSuchSectorException final : public MSXException {
public:
	explicit NoSuchSectorException(string_ref message)
		: MSXException(message) {}
};

class DiskIOErrorException final : public MSXException {
public:
	explicit DiskIOErrorException(string_ref message)
		: MSXException(message) {}
};

class DriveEmptyException final : public MSXException {
public:
	explicit DriveEmptyException(string_ref message)
		: MSXException(message) {}
};

class WriteProtectedException final : public MSXException {
public:
	explicit WriteProtectedException(string_ref message)
		: MSXException(message) {}
};

} // namespace openmsx

#endif
