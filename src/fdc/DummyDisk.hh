#ifndef FDCDUMMYBACKEND_HH
#define FDCDUMMYBACKEND_HH

#include "SectorBasedDisk.hh"

namespace openmsx {

class DummyDisk : public SectorBasedDisk
{
public:
	DummyDisk();
	virtual bool isDummyDisk() const;

private:
	virtual void readSectorImpl (size_t sector,       SectorBuffer& buf);
	virtual void writeSectorImpl(size_t sector, const SectorBuffer& buf);
	virtual bool isWriteProtectedImpl() const;
};

} // namespace openmsx

#endif
