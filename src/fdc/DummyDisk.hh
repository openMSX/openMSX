// $Id$

#ifndef FDCDUMMYBACKEND_HH
#define FDCDUMMYBACKEND_HH

#include "SectorBasedDisk.hh"

namespace openmsx {

class DummyDisk : public SectorBasedDisk
{
public:
	DummyDisk();

	virtual bool ready();
	virtual bool writeProtected();
	virtual bool doubleSided();

	virtual void readSectorImpl(unsigned sector, byte* buf);
	virtual void writeSectorImpl(unsigned sector, const byte* buf);
};

} // namespace openmsx

#endif
