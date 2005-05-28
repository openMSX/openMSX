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

	virtual void readLogicalSector(unsigned sector, byte* buf);
	virtual void writeLogicalSector(unsigned sector, const byte* buf);
};

} // namespace openmsx

#endif
