// $Id$

#ifndef RAMDSKDISKIMAGE_HH
#define RAMDSKDISKIMAGE_HH

#include "SectorBasedDisk.hh"

namespace openmsx {

class RamDSKDiskImage : public SectorBasedDisk
{
public:
	RamDSKDiskImage(unsigned size = 720 * 1024);
	virtual ~RamDSKDiskImage();

	virtual bool writeProtected();

private:
	virtual void readLogicalSector(unsigned sector, byte* buf);
	virtual void writeLogicalSector(unsigned sector, const byte* buf);

	byte* diskdata;
};

} // namespace openmsx

#endif
