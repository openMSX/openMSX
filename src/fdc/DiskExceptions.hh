#ifndef DISKEXCEPTIONS_HH
#define DISKEXCEPTIONS_HH

#include "MSXException.hh"

namespace openmsx {

class NoSuchSectorException final : public MSXException {
public:
	using MSXException::MSXException;
};

class DiskIOErrorException final : public MSXException {
public:
	using MSXException::MSXException;
};

class DriveEmptyException final : public MSXException {
public:
	using MSXException::MSXException;
};

class WriteProtectedException final : public MSXException {
public:
	using MSXException::MSXException;
};

} // namespace openmsx

#endif
