// $Id$

#ifndef RAMDSKDISKIMAGE_HH
#define RAMDSKDISKIMAGE_HH

#include "SectorBasedDisk.hh"
#include <memory>

namespace openmsx {

class RamDSKDiskImage : public SectorBasedDisk
{
public: 
	RamDSKDiskImage();
	RamDSKDiskImage(const int size);
	virtual ~RamDSKDiskImage();

	virtual void write(byte track, byte sector,
	                   byte side, unsigned size, const byte* buf);
	virtual bool writeProtected();
	virtual void read(byte track, byte sector, byte side, unsigned size, byte* buf);

private:
	virtual void readLogicalSector(unsigned sector, byte* buf);
	byte* diskdata;
};

} // namespace openmsx

#endif
