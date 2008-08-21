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

private:
	virtual void readSectorSBD(unsigned sector, byte* buf);
	virtual void writeSectorSBD(unsigned sector, const byte* buf);
	virtual bool isWriteProtectedImpl() const;
};

} // namespace openmsx

#endif
